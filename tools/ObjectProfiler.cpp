#if __BANK

#include "ObjectProfiler.h"
#include "system/AutoGPUCapture.h"
#include "renderer/DrawLists/DrawListMgr.h"
#include "camera/CamInterface.h"

#include "ObjectProfiler_parser.h"

//DON'T COMMIT
//OPTIMISATIONS_OFF()

#define TIME_FOR_ONE_ROTATION	(10.0f)		// Seconds
#define LOD_CROSSFADE_THRESHOLD	(5.0f)		// m

// Externs, currently using debug functionality to display objects
extern	RegdEnt gpDisplayObject;			// Debug.cpp	-	The display object
extern	int		gCurrentDisplayObjectCount;	// Debug.cpp	-	The number of objects (0 is no object)
extern	int		gCurrentDisplayObjectIndex;	// Debug.cpp	-	Set this and call....
extern	void	SelectDisplayObject();		// Debug.cpp	-	Sets the object to display based on gCurrentDisplayObjectIndex
extern	void	DeleteObject();				// Debug.cpp
extern	Vector3 gDisplayOffset;				// Debug.cpp	-	Is added on to the spawn point offset from created objects

float	CObjectProfiler::m_CurrentRotAngle;
int		CObjectProfiler::m_CurrentObjectIDX;
int		CObjectProfiler::m_CurrentObjectLodIDX;
int		CObjectProfiler::m_ObjProfileState;
bool	CObjectProfiler::m_ObjProfileActive = false;
ObjectProfilerResults CObjectProfiler::m_Results;

MetricsCapture::SampledValue<float> CaptureVar;

void CObjectProfiler::Init()
{
	m_CurrentObjectIDX = 1;
	m_CurrentRotAngle = 0.0f;
	m_ObjProfileState = STATE_SET_TO_DISPLAY;
	m_ObjProfileActive = true;

	const char* cVersionNumber = CDebug::GetVersionNumber();
	m_Results.buildVersion = cVersionNumber;
}

bool CObjectProfiler::Process()
{
	switch(m_ObjProfileState)
	{
		case STATE_SET_TO_DISPLAY:
		{
			if( m_CurrentObjectIDX < gCurrentDisplayObjectCount )
			{
				m_CurrentObjectLodIDX = 0;
				gCurrentDisplayObjectIndex = m_CurrentObjectIDX;

				// Needed because when spawning large objects they sometimes hit the camera and cause it to move.
				Vector3 forward = camInterface::GetFront();
				gDisplayOffset = 3.0f * forward;

				SelectDisplayObject();

				if( gpDisplayObject.Get())
				{
					m_ObjProfileState = STATE_SET_TO_WAIT_FOR_DRAWABLE;

					// Create an entry for this object
					ProfiledObjectData thisObjectData;
					thisObjectData.objectName = gpDisplayObject->GetModelName();
					m_Results.objectData.Grow() = thisObjectData;
				}
				else
				{
					// Unknown model, don't try to load and render it!, Skip onto the next.
					m_ObjProfileState = STATE_SET_TO_CLEANUP;
				}
			}
			else
			{
				// Finished :)
				const char *platform = "PC";
#if __XENON
				platform = "Xbox360";
#elif __PS3
				platform = "PS3";
#endif
				// Write out the storage data
				char	buffer[256];
				sprintf(buffer,"x:/%s_ObjProfile", platform);

				INIT_PARSER;
				PARSER.SaveObject(buffer, "xml", &m_Results, parManager::XML);

				// and reset it.
				m_Results.Reset();

				// End the process
				m_ObjProfileState = STATE_SET_TO_COMPLETE;
				gpDisplayObject = NULL;
			}
		}
		break;

		case	STATE_SET_TO_WAIT_FOR_DRAWABLE:
		{
			rmcDrawable* pDrawable = gpDisplayObject->GetDrawable();
			if(pDrawable)
			{
				m_ObjProfileState = STATE_SET_TO_PLACE_FOR_LOD;
			}
		}
		break;

		case	STATE_SET_TO_PLACE_FOR_LOD:
		{
			rmcDrawable* pDrawable = gpDisplayObject->GetDrawable();
			const rmcLodGroup& lodGroup = pDrawable->GetLodGroup();
			
			if(m_CurrentObjectLodIDX < rage::LOD_COUNT && lodGroup.ContainsLod(m_CurrentObjectLodIDX))
			{
				// Get the current camera position
				Vector3 forward = camInterface::GetFront();
				Vector3 pos = camInterface::GetPos();
				Vector3	up = camInterface::GetUp();
				float nearClip = camInterface::GetNear();

				// Get the model radius
				float yOffset = 0.0f;
				float radius = 0.0f;

				//Stuff for trying to get the ped to centre.
				CBaseModelInfo* pModelInfo = gpDisplayObject->GetBaseModelInfo();
				if(pModelInfo && pModelInfo->GetModelType()==MI_TYPE_PED)
				{
					// Peds're a bit weird because the modelAABB returned by each lod isn't correct, I presume this is because it's a skinned model so the AABB changes
					// use the entities AABB, this should be OK since I know the origin of the geometry is the pelvis (roughly the centre)
					spdAABB	tempAABB;
					gpDisplayObject->GetAABB(tempAABB);
					Vector3 bBoxCentre = VEC3V_TO_VECTOR3(tempAABB.GetCenter());
					yOffset = 0.0f;
					radius = (bBoxCentre - tempAABB.GetMinVector3()).Mag();
				}
				else
				{
					const spdAABB *pModelSpaceBox = &lodGroup.GetLodModel0(m_CurrentObjectLodIDX).GetModelAABB();
					Vector3 bBoxCentre = VEC3V_TO_VECTOR3(pModelSpaceBox->GetCenter());
					yOffset = bBoxCentre.z;	
					radius = (bBoxCentre - pModelSpaceBox->GetMinVector3()).Mag();
				}

				// Get the min distance we need to get this object totally on screen
				float tanTheta = camInterface::GetViewportTanFovV();
				float theta = atan(tanTheta);

				// Slight inaccuracies cause it to clip the near plane
				theta *= 0.90f;

				float dist = radius / Sinf(theta);

				// Place at appropriate distance from camera
#if __ASSERT
				float theMax = lodGroup.GetLodThresh(m_CurrentObjectLodIDX) - LOD_CROSSFADE_THRESHOLD;
#endif
				if ( m_CurrentObjectLodIDX == 0)
				{
					if( dist - radius < nearClip )
					{
						dist = nearClip + radius;
					}
				
				}
				else
				{
					float theMin = lodGroup.GetLodThresh(m_CurrentObjectLodIDX-1) + LOD_CROSSFADE_THRESHOLD;

					if( dist <= theMin )
					{
						dist = theMin;
					}
				}
				Assertf(dist <= theMax, "Error, LOD can't completely fit on screen at current LOD Ranges");
				// Set position

				Vector3 posForLod = pos + (dist * forward) - (up * yOffset);
				gpDisplayObject->SetPosition(posForLod, true);

				// Turn off physics to prevent it moving
				if( gpDisplayObject->GetIsPhysical() )
				{
					CPhysical *pPhysical = static_cast<CPhysical*>(gpDisplayObject.Get());
					pPhysical->DisableCollision(NULL, true);
					pPhysical->SetFixedPhysics(true);
				}

				// Rotate
				m_CurrentRotAngle = 0.0f;
				m_ObjProfileState = STATE_SET_TO_ROTATE;

				// Reset the captureVar
				CaptureVar.Reset();
			}
			else
			{
				// No more lods, next model
				m_ObjProfileState = STATE_SET_TO_CLEANUP;
			}
		}
		break;
	
		case STATE_SET_TO_ROTATE:
		{
			float rotDelta = TWO_PI * (fwTimer::GetTimeStep() / TIME_FOR_ONE_ROTATION);
			m_CurrentRotAngle += rotDelta;
			if(m_CurrentRotAngle >= TWO_PI)
			{
				// Done one rotation

				// Save the data in CaptureVar for this LOD here
				ProfiledObjectLODData theLodData;
				theLodData.lod = m_CurrentObjectLodIDX;
				theLodData.min = CaptureVar.Min();
				theLodData.max = CaptureVar.Max();
				theLodData.average = CaptureVar.Mean();
				theLodData.std = CaptureVar.StandardDeviation();

				m_Results.objectData[m_CurrentObjectIDX-1].objectLODData.Grow() = theLodData;	// -1 'cos we start at object 1 (0 = no object)

				m_CurrentObjectLodIDX++;
				m_ObjProfileState = STATE_SET_TO_PLACE_FOR_LOD;
			}
			else
			{
				Matrix34 rotMat;
				rotMat.Identity();
				rotMat.RotateZ(m_CurrentRotAngle);
				rotMat.d = VEC3V_TO_VECTOR3(::gpDisplayObject->GetTransform().GetPosition());
				// Push rotation into the displayed object
				gpDisplayObject->SetMatrix(rotMat, true);

				// Get the previous frames GPU data into CaptureVar
				CaptureVar.AddSample(gDrawListMgr->GetDrawListGPUTime(DL_RENDERPHASE_GBUF));	// Can I use the renderphase index? not sure it's right.
			}
		}
		break;

		case STATE_SET_TO_CLEANUP:
		{
			DeleteObject();

			m_CurrentObjectIDX++;		// Next object
			m_ObjProfileState = STATE_SET_TO_DISPLAY;
			// Call ourselves again so we don't wait a frame for the next object to appear
			Process();
		}
		break;

		case STATE_SET_TO_COMPLETE:
		{
			m_ObjProfileActive = false;
		}
		break;
	}

	return !m_ObjProfileActive;
}

#endif	//__BANK