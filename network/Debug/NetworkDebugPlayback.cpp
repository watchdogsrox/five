//
// name:        NetworkDebugPlayback.cpp
// description: Allows loading of an external file to playback game entity motion in-game
// written by:  Daniel Yelland
//

#if __BANK

#include "network/Debug/NetworkDebugPlayback.h"

#include "camera/CamInterface.h"
#include "camera/debug/DebugDirector.h"
#include "grcore/debugdraw.h"
#include "peds/Ped.h"
#include "peds/PedPopulation.h"
#include "shaders/CustomShaderEffectVehicle.h"
#include "streaming/streaming.h"
#include "system/ControlMgr.h"
#include "system/FileMgr.h"
#include "vehicles/VehicleFactory.h"

NETWORK_OPTIMISATIONS()

CNetworkDebugPlayback::PedData     CNetworkDebugPlayback::m_PedPlaybackData;
CNetworkDebugPlayback::VehicleData CNetworkDebugPlayback::m_VehiclePlaybackData;
bool                               CNetworkDebugPlayback::m_ShowDebugText;

void CNetworkDebugPlayback::Init()
{
    m_PedPlaybackData.ResetSnapshotData();
    m_VehiclePlaybackData.ResetSnapshotData();

    m_ShowDebugText = false;
}

void CNetworkDebugPlayback::Shutdown()
{
}

void CNetworkDebugPlayback::Update()
{
    if(m_PedPlaybackData.m_Playing)
    {
        if(m_PedPlaybackData.m_CurrentFrame < m_PedPlaybackData.m_NumSnapshots)
        {
            UpdateObjects();
            m_PedPlaybackData.m_CurrentFrame++;
        }
        else
        {
            m_PedPlaybackData.m_Playing = false;
        }
    }

    if(m_VehiclePlaybackData.m_Playing)
    {
        if(m_VehiclePlaybackData.m_CurrentFrame < m_VehiclePlaybackData.m_NumSnapshots)
        {
            UpdateObjects();
            m_VehiclePlaybackData.m_CurrentFrame++;
        }
        else
        {
            m_VehiclePlaybackData.m_Playing = false;
        }
    }

    if(m_VehiclePlaybackData.m_LocalVehicle && m_VehiclePlaybackData.m_CloneVehicle)
    {
        m_VehiclePlaybackData.m_LocalVehicle->SetIsVisibleForModule(SETISVISIBLE_MODULE_NETWORK, m_VehiclePlaybackData.m_ShowLocalVehicle, true);
        m_VehiclePlaybackData.m_CloneVehicle->SetIsVisibleForModule(SETISVISIBLE_MODULE_NETWORK, m_VehiclePlaybackData.m_ShowCloneVehicle, true);
    }

    if(m_PedPlaybackData.m_RenderLocalDebugVectors && m_PedPlaybackData.m_LocalPed)
    {
        Vector3 forwardDir  = VEC3V_TO_VECTOR3(m_PedPlaybackData.m_LocalPed->GetTransform().GetB());
        Vector3 movementDir = forwardDir;

        if(m_PedPlaybackData.m_CurrentFrame > 0 && m_PedPlaybackData.m_CurrentFrame < m_PedPlaybackData.m_NumSnapshots)
        {
            movementDir = m_PedPlaybackData.m_LocalSnapshots[m_PedPlaybackData.m_CurrentFrame].m_Position - m_PedPlaybackData.m_LocalSnapshots[m_PedPlaybackData.m_CurrentFrame-1].m_Position;
            movementDir.Normalize();
        }

        Vector3 pedPosition = VEC3V_TO_VECTOR3(m_PedPlaybackData.m_LocalPed->GetTransform().GetPosition());
        grcDebugDraw::Line(pedPosition, pedPosition + forwardDir,  Color_green, Color_green, 1);
        grcDebugDraw::Line(pedPosition, pedPosition + movementDir, Color_blue,  Color_blue,  1);
    }

    if(m_PedPlaybackData.m_RenderCloneDebugVectors && m_PedPlaybackData.m_ClonePed)
    {
        Vector3 forwardDir  = VEC3V_TO_VECTOR3(m_PedPlaybackData.m_ClonePed->GetTransform().GetB());
        Vector3 movementDir = forwardDir;

        if(m_PedPlaybackData.m_CurrentFrame > 0 && m_PedPlaybackData.m_CurrentFrame < m_PedPlaybackData.m_NumSnapshots)
        {
            movementDir = m_PedPlaybackData.m_CloneSnapshots[m_PedPlaybackData.m_CurrentFrame].m_Position - m_PedPlaybackData.m_CloneSnapshots[m_PedPlaybackData.m_CurrentFrame-1].m_Position;
            movementDir.Normalize();
        }

        Vector3 pedPosition = VEC3V_TO_VECTOR3(m_PedPlaybackData.m_ClonePed->GetTransform().GetPosition());
        grcDebugDraw::Line(pedPosition, pedPosition + forwardDir,  Color_orange, Color_orange,   1);
        grcDebugDraw::Line(pedPosition, pedPosition + movementDir, Color_yellow,  Color_yellow,  1);
    }

    // vehicle debug
    TUNE_FLOAT(ARROW_HEAD_SIZE, 0.2f, 0.1f, 1.0f, 0.05f);

    if(m_VehiclePlaybackData.m_NumSnapshots > 0 && m_VehiclePlaybackData.m_CurrentFrame < m_VehiclePlaybackData.m_NumSnapshots)
    {
        if(m_VehiclePlaybackData.m_DisplayLocalForwardVector && m_VehiclePlaybackData.m_LocalVehicle)
        {
            Vector3 vehiclePosition = VEC3V_TO_VECTOR3(m_VehiclePlaybackData.m_LocalVehicle->GetTransform().GetPosition());
            Vector3 forwardDir      = VEC3V_TO_VECTOR3(m_VehiclePlaybackData.m_LocalVehicle->GetTransform().GetB());

            TUNE_FLOAT(LOCAL_FORWARD_VECTOR_SCALE, 3.0f, 1.0f, 10.0f, 0.5f);
            forwardDir.Scale(LOCAL_FORWARD_VECTOR_SCALE);

            grcDebugDraw::Arrow(VECTOR3_TO_VEC3V(vehiclePosition), VECTOR3_TO_VEC3V(vehiclePosition + forwardDir), ARROW_HEAD_SIZE, Color_green, 1);
        }

        if(m_VehiclePlaybackData.m_DisplayCloneForwardVector && m_VehiclePlaybackData.m_CloneVehicle)
        {
            Vector3 vehiclePosition = VEC3V_TO_VECTOR3(m_VehiclePlaybackData.m_CloneVehicle->GetTransform().GetPosition());
            Vector3 forwardDir      = VEC3V_TO_VECTOR3(m_VehiclePlaybackData.m_CloneVehicle->GetTransform().GetB());

            TUNE_FLOAT(CLONE_FORWARD_VECTOR_SCALE, 3.0f, 1.0f, 10.0f, 0.5f);
            forwardDir.Scale(CLONE_FORWARD_VECTOR_SCALE);

            grcDebugDraw::Arrow(VECTOR3_TO_VEC3V(vehiclePosition), VECTOR3_TO_VEC3V(vehiclePosition + forwardDir), ARROW_HEAD_SIZE, Color_orange, 1);
        }

        if(m_VehiclePlaybackData.m_DisplayLocalVelocityVector && m_VehiclePlaybackData.m_LocalVehicle)
        {
            Vector3 vehiclePosition    = VEC3V_TO_VECTOR3(m_VehiclePlaybackData.m_LocalVehicle->GetTransform().GetPosition());
            Vector3 vehicleVelocityDir = m_VehiclePlaybackData.m_LocalSnapshots[m_VehiclePlaybackData.m_CurrentFrame].m_Velocity;
            vehicleVelocityDir.Normalize();

            TUNE_FLOAT(LOCAL_VEL_VECTOR_SCALE, 3.0f, 1.0f, 10.0f, 0.5f);
            vehicleVelocityDir.Scale(LOCAL_VEL_VECTOR_SCALE);

            grcDebugDraw::Arrow(VECTOR3_TO_VEC3V(vehiclePosition), VECTOR3_TO_VEC3V(vehiclePosition + vehicleVelocityDir), ARROW_HEAD_SIZE,  Color_blue,  1);
        }

        if(m_VehiclePlaybackData.m_DisplayCloneVelocityVector && m_VehiclePlaybackData.m_CloneVehicle)
        {
            Vector3 vehiclePosition    = VEC3V_TO_VECTOR3(m_VehiclePlaybackData.m_CloneVehicle->GetTransform().GetPosition());
            Vector3 vehicleVelocityDir = m_VehiclePlaybackData.m_CloneSnapshots[m_VehiclePlaybackData.m_CurrentFrame].m_Velocity;
            vehicleVelocityDir.Normalize();

            TUNE_FLOAT(CLONE_VEL_VECTOR_SCALE, 3.0f, 1.0f, 10.0f, 0.5f);
            vehicleVelocityDir.Scale(CLONE_VEL_VECTOR_SCALE);

            grcDebugDraw::Arrow(VECTOR3_TO_VEC3V(vehiclePosition), VECTOR3_TO_VEC3V(vehiclePosition + vehicleVelocityDir), ARROW_HEAD_SIZE,  Color_yellow,  1);
        }

        if(m_VehiclePlaybackData.m_DisplayLocalMovementVector && m_VehiclePlaybackData.m_LocalVehicle && m_VehiclePlaybackData.m_CurrentFrame > 0)
        {
            Vector3 vehiclePosition    = VEC3V_TO_VECTOR3(m_VehiclePlaybackData.m_LocalVehicle->GetTransform().GetPosition());
            Vector3 vehicleMovementDir = m_VehiclePlaybackData.m_LocalSnapshots[m_VehiclePlaybackData.m_CurrentFrame].m_Position - m_VehiclePlaybackData.m_LocalSnapshots[m_VehiclePlaybackData.m_CurrentFrame-1].m_Position;
            vehicleMovementDir.Normalize();

            TUNE_FLOAT(LOCAL_MOVE_VECTOR_SCALE, 3.0f, 1.0f, 10.0f, 0.5f);
            vehicleMovementDir.Scale(LOCAL_MOVE_VECTOR_SCALE);

            grcDebugDraw::Arrow(VECTOR3_TO_VEC3V(vehiclePosition), VECTOR3_TO_VEC3V(vehiclePosition + vehicleMovementDir), ARROW_HEAD_SIZE,  Color_purple,  1);
        }

        if(m_VehiclePlaybackData.m_DisplayCloneMovementVector && m_VehiclePlaybackData.m_CloneVehicle && m_VehiclePlaybackData.m_CurrentFrame > 0)
        {
            Vector3 vehiclePosition    = VEC3V_TO_VECTOR3(m_VehiclePlaybackData.m_CloneVehicle->GetTransform().GetPosition());
            Vector3 vehicleMovementDir = m_VehiclePlaybackData.m_CloneSnapshots[m_VehiclePlaybackData.m_CurrentFrame].m_Position - m_VehiclePlaybackData.m_CloneSnapshots[m_VehiclePlaybackData.m_CurrentFrame-1].m_Position;
            vehicleMovementDir.Normalize();

            TUNE_FLOAT(CLONE_MOVE_VECTOR_SCALE, 3.0f, 1.0f, 10.0f, 0.5f);
            vehicleMovementDir.Scale(CLONE_MOVE_VECTOR_SCALE);

            grcDebugDraw::Arrow(VECTOR3_TO_VEC3V(vehiclePosition), VECTOR3_TO_VEC3V(vehiclePosition + vehicleMovementDir), ARROW_HEAD_SIZE,  Color_pink,  1);
        }
    }

    if(m_ShowDebugText)
    {
        DisplayDebugText();
    }
}

void CNetworkDebugPlayback::LoadPedPlaybackFile()
{
    m_PedPlaybackData.ResetSnapshotData();

    m_PedPlaybackData.m_ShowLocalPed = true;
    m_PedPlaybackData.m_ShowClonePed = true;
    m_PedPlaybackData.m_Playing      = false;
    m_PedPlaybackData.m_CurrentFrame = 0;

    FileHandle hFile = CFileMgr::OpenFile("NetworkDebugPedPlayback.csv");

    if(hFile == INVALID_FILE_HANDLE)
    {
        gnetDebug2("Failed to open NetworkDebugPedPlayback.csv!");
    }
    else
    {
        char buffer[256] = {0};

        while(CFileMgr::ReadLine(hFile, buffer, sizeof(buffer)))
        {
            Vector3 localPosition;
            Vector3 clonePosition;
            float   localHeading;
            float   cloneHeading;
            int numValuesRead = sscanf(buffer, "%f,%f,%f,%f,%f,%f,%f,%f", &localPosition.x, &localPosition.y, &localPosition.z, &localHeading,
                                                                          &clonePosition.x, &clonePosition.y, &clonePosition.z, &cloneHeading);

            const int NUM_EXPECTED_VALUES = 8; 
            if(numValuesRead != NUM_EXPECTED_VALUES || m_PedPlaybackData.m_NumSnapshots >= PedData::MAX_PED_SNAPSHOTS)
            {
                break;
            }

            m_PedPlaybackData.AddSnapshot(localPosition, localHeading, clonePosition, cloneHeading);
        }

        CFileMgr::CloseFile(hFile);

        if(m_PedPlaybackData.m_NumSnapshots > 0)
        {
            // create the peds for playing back if they don't exist
            if(m_PedPlaybackData.m_LocalPed == 0)
            {
                fwModelId modelId = CModelInfo::GetModelIdFromName("S_M_Y_Dealer_01");
                m_PedPlaybackData.m_LocalPed = CreatePed(modelId, m_PedPlaybackData.m_LocalSnapshots[0].m_Position, m_PedPlaybackData.m_LocalSnapshots[0].m_Heading);
            }

            if(m_PedPlaybackData.m_ClonePed == 0)
            {
                fwModelId modelId = CModelInfo::GetModelIdFromName("s_m_y_mime");
                m_PedPlaybackData.m_ClonePed = CreatePed(modelId, m_PedPlaybackData.m_CloneSnapshots[0].m_Position, m_PedPlaybackData.m_CloneSnapshots[0].m_Heading);
            }

            camDebugDirector& debugDirector = camInterface::GetDebugDirector();
            debugDirector.ActivateFreeCam();

            //Move free camera to desired place.
            camFrame& freeCamFrame = debugDirector.GetFreeCamFrameNonConst();

            static float y = 10.0f;
            static float z = 0.5f;

            Vector3 focusPos = m_PedPlaybackData.m_LocalSnapshots[0].m_Position;
            Vector3 camPos = focusPos + Vector3(0.0f, y, y*z);
            Vector3 camDir = focusPos - camPos;

            freeCamFrame.SetPosition(camPos);
            freeCamFrame.SetWorldMatrixFromFront(camDir);

            CControlMgr::SetDebugPadOn(true);
        }
    }
}

void CNetworkDebugPlayback::PlayPedData()
{
    m_PedPlaybackData.m_Playing = true;
}

void CNetworkDebugPlayback::PausePedData()
{
    m_PedPlaybackData.m_Playing = false;
}

void CNetworkDebugPlayback::StopPedData()
{
    m_PedPlaybackData.m_CurrentFrame = 0;
    m_PedPlaybackData.m_Playing      = false;
}

void CNetworkDebugPlayback::StepForwardPedData()
{
    if(m_PedPlaybackData.m_CurrentFrame < m_PedPlaybackData.m_NumSnapshots)
    {
        m_PedPlaybackData.m_CurrentFrame++;
        CNetworkDebugPlayback::UpdateObjects();
    }
}

void CNetworkDebugPlayback::StepBackwardPedData()
{
    if(m_PedPlaybackData.m_CurrentFrame > 0)
    {
        m_PedPlaybackData.m_CurrentFrame--;
        CNetworkDebugPlayback::UpdateObjects();
    }
}

void CNetworkDebugPlayback::LoadVehiclePlaybackFile()
{
    m_VehiclePlaybackData.ResetSnapshotData();

    m_VehiclePlaybackData.m_ShowLocalVehicle = true;
    m_VehiclePlaybackData.m_ShowCloneVehicle = true;
    m_VehiclePlaybackData.m_Playing          = false;
    m_VehiclePlaybackData.m_CurrentFrame     = 0;

    FileHandle hFile = CFileMgr::OpenFile("NetworkDebugVehiclePlayback.csv");

    if(hFile == INVALID_FILE_HANDLE)
    {
        gnetDebug2("Failed to open NetworkDebugVehiclePlayback.csv!");
    }
    else
    {
        char buffer[256] = {0};

        while(CFileMgr::ReadLine(hFile, buffer, sizeof(buffer)))
        {
            Vector3 localPosition;
            Vector3 localVelocity;
            Vector3 localEulers;
            Vector3 clonePosition;
            Vector3 cloneVelocity;
            Vector3 cloneEulers;
            int numValuesRead = sscanf(buffer, "%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f",
                                &localPosition.x, &localPosition.y, &localPosition.z,
                                &localVelocity.x, &localVelocity.y, &localVelocity.z,
                                &localEulers.x,   &localEulers.y,   &localEulers.z,
                                &clonePosition.x, &clonePosition.y, &clonePosition.z,
                                &cloneVelocity.x, &cloneVelocity.y, &cloneVelocity.z,
                                &cloneEulers.x,   &cloneEulers.y,   &cloneEulers.z);

            const int NUM_EXPECTED_VALUES = 18; 
            if(numValuesRead != NUM_EXPECTED_VALUES || m_VehiclePlaybackData.m_NumSnapshots >= VehicleData::MAX_VEHICLE_SNAPSHOTS)
            {
                break;
            }

            m_VehiclePlaybackData.AddSnapshot(localPosition, localVelocity, localEulers, clonePosition, cloneVelocity, cloneEulers);
        }

        CFileMgr::CloseFile(hFile);

        if(m_VehiclePlaybackData.m_NumSnapshots > 0)
        {
            // create the vehicles for playing back if they don't exist
            if(m_VehiclePlaybackData.m_LocalVehicle == 0)
            {
                fwModelId modelId = CModelInfo::GetModelIdFromName("EMPEROR");
                m_VehiclePlaybackData.m_LocalVehicle = CreateVehicle(modelId, m_VehiclePlaybackData.m_LocalSnapshots[0].m_Position, m_VehiclePlaybackData.m_LocalSnapshots[0].m_Orientation);

                if(m_VehiclePlaybackData.m_LocalVehicle)
                {
                    gnetAssert(m_VehiclePlaybackData.m_LocalVehicle->GetDrawHandlerPtr() != NULL);
                    CCustomShaderEffectVehicle* pShader = static_cast<CCustomShaderEffectVehicle*>(m_VehiclePlaybackData.m_LocalVehicle->GetDrawHandler().GetShaderEffect());
                    gnetAssert(pShader);

                    const Color32 col(0,0,255);
                    pShader->SetCustomPrimaryColor(col);
                }
            }

            if(m_VehiclePlaybackData.m_CloneVehicle == 0)
            {
                fwModelId modelId = CModelInfo::GetModelIdFromName("EMPEROR");
                m_VehiclePlaybackData.m_CloneVehicle = CreateVehicle(modelId, m_VehiclePlaybackData.m_CloneSnapshots[0].m_Position, m_VehiclePlaybackData.m_CloneSnapshots[0].m_Orientation);

                if(m_VehiclePlaybackData.m_CloneVehicle)
                {
                    gnetAssert(m_VehiclePlaybackData.m_CloneVehicle->GetDrawHandlerPtr() != NULL);
                    CCustomShaderEffectVehicle* pShader = static_cast<CCustomShaderEffectVehicle*>(m_VehiclePlaybackData.m_CloneVehicle->GetDrawHandler().GetShaderEffect());
                    gnetAssert(pShader);

                    const Color32 col(255,0,0);
                    pShader->SetCustomPrimaryColor(col);
                }
            }

            camDebugDirector& debugDirector = camInterface::GetDebugDirector();
            debugDirector.ActivateFreeCam();

            //Move free camera to desired place.
            camFrame& freeCamFrame = debugDirector.GetFreeCamFrameNonConst();

            static float y = 10.0f;
            static float z = 0.5f;

            Vector3 focusPos = m_VehiclePlaybackData.m_LocalSnapshots[0].m_Position;
            Vector3 camPos = focusPos + Vector3(0.0f, y, y*z);
            Vector3 camDir = focusPos - camPos;

            freeCamFrame.SetPosition(camPos);
            freeCamFrame.SetWorldMatrixFromFront(camDir);

            CControlMgr::SetDebugPadOn(true);
        }
    }

	CNetworkDebugPlayback::UpdateObjects();
}

void CNetworkDebugPlayback::PlayVehicleData()
{
	if (m_VehiclePlaybackData.m_CurrentFrame == m_VehiclePlaybackData.m_NumSnapshots)
	{
		StopVehicleData();
	}
    m_VehiclePlaybackData.m_Playing = true;
}

void CNetworkDebugPlayback::PauseVehicleData()
{
    m_VehiclePlaybackData.m_Playing = false;
}

void CNetworkDebugPlayback::StopVehicleData()
{
    m_VehiclePlaybackData.m_CurrentFrame = 0;
    m_VehiclePlaybackData.m_Playing      = false;

    UpdateObjects();
}

void CNetworkDebugPlayback::StepForwardVehicleData()
{
    if(m_VehiclePlaybackData.m_CurrentFrame < m_VehiclePlaybackData.m_NumSnapshots)
    {
        m_VehiclePlaybackData.m_CurrentFrame++;
        CNetworkDebugPlayback::UpdateObjects();
    }
}

void CNetworkDebugPlayback::StepBackwardVehicleData()
{
    if(m_VehiclePlaybackData.m_CurrentFrame > 0)
    {
        m_VehiclePlaybackData.m_CurrentFrame--;
        CNetworkDebugPlayback::UpdateObjects();
    }
}

void CNetworkDebugPlayback::UpdateObjects()
{
    if(m_PedPlaybackData.m_NumSnapshots > 0)
    {
        if(m_PedPlaybackData.m_LocalPed && m_PedPlaybackData.m_ClonePed)
        {
            if(m_PedPlaybackData.m_CurrentFrame < m_PedPlaybackData.m_NumSnapshots)
            {
                m_PedPlaybackData.m_LocalPed->Teleport(m_PedPlaybackData.m_LocalSnapshots[m_PedPlaybackData.m_CurrentFrame].m_Position, m_PedPlaybackData.m_LocalSnapshots[m_PedPlaybackData.m_CurrentFrame].m_Heading);
                m_PedPlaybackData.m_ClonePed->Teleport(m_PedPlaybackData.m_CloneSnapshots[m_PedPlaybackData.m_CurrentFrame].m_Position, m_PedPlaybackData.m_CloneSnapshots[m_PedPlaybackData.m_CurrentFrame].m_Heading);
                m_PedPlaybackData.m_LocalPed->SetIsVisibleForModule(SETISVISIBLE_MODULE_NETWORK, m_PedPlaybackData.m_ShowLocalPed, true);
                m_PedPlaybackData.m_ClonePed->SetIsVisibleForModule(SETISVISIBLE_MODULE_NETWORK, m_PedPlaybackData.m_ShowClonePed, true);
            }
        }
    }

    if(m_VehiclePlaybackData.m_NumSnapshots > 0)
    {
        if(m_VehiclePlaybackData.m_LocalVehicle && m_VehiclePlaybackData.m_CloneVehicle)
        {
            if(m_VehiclePlaybackData.m_CurrentFrame < m_VehiclePlaybackData.m_NumSnapshots)
            {
                Matrix34 localMatrix = m_VehiclePlaybackData.m_LocalSnapshots[m_VehiclePlaybackData.m_CurrentFrame].m_Orientation;
                localMatrix.d        = m_VehiclePlaybackData.m_LocalSnapshots[m_VehiclePlaybackData.m_CurrentFrame].m_Position;
                m_VehiclePlaybackData.m_LocalVehicle->SetMatrix(localMatrix, true, true, true);

                Matrix34 cloneMatrix = m_VehiclePlaybackData.m_CloneSnapshots[m_VehiclePlaybackData.m_CurrentFrame].m_Orientation;
                cloneMatrix.d        = m_VehiclePlaybackData.m_CloneSnapshots[m_VehiclePlaybackData.m_CurrentFrame].m_Position;
                m_VehiclePlaybackData.m_CloneVehicle->SetMatrix(cloneMatrix, true, true, true);
            }
        }
    }
}

CPed *CNetworkDebugPlayback::CreatePed(fwModelId modelId, const Vector3 &position, const float heading)
{
    if(!CModelInfo::HaveAssetsLoaded(modelId))
    {
        CModelInfo::RequestAssets(modelId, STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD);
        CStreaming::LoadAllRequestedObjects();
    }

    gnetAssert(CModelInfo::HaveAssetsLoaded(modelId));

    Matrix34 tempMat;
    tempMat.d = position;
    tempMat.FromEulersXYZ(Vector3(0.0f, 0.0f, heading));
    const CControlledByInfo networkNpcControl(true, false);
    CPed *newPed = CPedFactory::GetFactory()->CreatePed(networkNpcControl, modelId, &tempMat, true, false, false);

    if(newPed)
    {
        CGameWorld::Add(newPed, CGameWorld::OUTSIDE );

        newPed->GetPortalTracker()->SetProbeType(CPortalTracker::PROBE_TYPE_NEAR);

        newPed->GetPortalTracker()->RequestRescanNextUpdate();
        newPed->GetPortalTracker()->Update(VEC3V_TO_VECTOR3(newPed->GetTransform().GetPosition()));

        newPed->PopTypeSet(POPTYPE_MISSION);

        CPedPopulation::AddPedToPopulationCount(newPed);

        newPed->SetFixedPhysics(true);
        newPed->DisableCollision();
    }

    return newPed;
}

CVehicle *CNetworkDebugPlayback::CreateVehicle(fwModelId modelId, const Vector3 &position, const Matrix34 &localOrientation)
{
    if(!CModelInfo::HaveAssetsLoaded(modelId))
    {
        CModelInfo::RequestAssets(modelId, STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD);
        CStreaming::LoadAllRequestedObjects();
    }

    gnetAssertf(CModelInfo::HaveAssetsLoaded(modelId), "Failed to stream required model for debug playback!");

    Matrix34 createMatrix = localOrientation;
    createMatrix.d        = position;

    CVehicle *newVehicle = CVehicleFactory::GetFactory()->Create(modelId, ENTITY_OWNEDBY_DEBUG, POPTYPE_MISSION, &createMatrix, false);

    if(newVehicle)
    {
        CGameWorld::Add(newVehicle, CGameWorld::OUTSIDE );

        newVehicle->GetPortalTracker()->SetProbeType(CPortalTracker::PROBE_TYPE_NEAR);

        newVehicle->GetPortalTracker()->RequestRescanNextUpdate();
        newVehicle->GetPortalTracker()->Update(VEC3V_TO_VECTOR3(newVehicle->GetTransform().GetPosition()));

        newVehicle->PopTypeSet(POPTYPE_MISSION);

        newVehicle->SetFixedPhysics(true);
        newVehicle->DisableCollision();
    }

    return newVehicle;
}

void CNetworkDebugPlayback::DisplayDebugText()
{
    if(m_PedPlaybackData.m_NumSnapshots > 0 && m_PedPlaybackData.m_CurrentFrame < m_PedPlaybackData.m_NumSnapshots)
    {
        grcDebugDraw::AddDebugOutput("Ped Data:");
        grcDebugDraw::AddDebugOutput("");
        grcDebugDraw::AddDebugOutput("Current frame   %d/%d", m_PedPlaybackData.m_CurrentFrame, m_PedPlaybackData.m_NumSnapshots);
        grcDebugDraw::AddDebugOutput("Position Delta: %.2f", (m_PedPlaybackData.m_LocalSnapshots[m_PedPlaybackData.m_CurrentFrame].m_Position - m_PedPlaybackData.m_CloneSnapshots[m_PedPlaybackData.m_CurrentFrame].m_Position).Mag2());
        grcDebugDraw::AddDebugOutput("Heading Delta:  %.2f", fabs(m_PedPlaybackData.m_LocalSnapshots[m_PedPlaybackData.m_CurrentFrame].m_Heading - m_PedPlaybackData.m_CloneSnapshots[m_PedPlaybackData.m_CurrentFrame].m_Heading));
    }

    if(m_VehiclePlaybackData.m_NumSnapshots > 0 && m_VehiclePlaybackData.m_CurrentFrame < m_VehiclePlaybackData.m_NumSnapshots)
    {
        grcDebugDraw::AddDebugOutput("Vehicle Data:");
        grcDebugDraw::AddDebugOutput("");
        grcDebugDraw::AddDebugOutput("Current frame   %d/%d", m_VehiclePlaybackData.m_CurrentFrame, m_VehiclePlaybackData.m_NumSnapshots);
        grcDebugDraw::AddDebugOutput("Position Delta: %.2f", (m_VehiclePlaybackData.m_LocalSnapshots[m_VehiclePlaybackData.m_CurrentFrame].m_Position - m_VehiclePlaybackData.m_CloneSnapshots[m_VehiclePlaybackData.m_CurrentFrame].m_Position).Mag2());
    }
}

void CNetworkDebugPlayback::AddDebugWidgets()
{
    bkBank *bank = BANKMGR.FindBank("Network");

    if(gnetVerifyf(bank, "Unable to find network bank!"))
    {
        bank->PushGroup("Network Debug Playback", false);
        {
            bank->AddToggle("Show Debug Text", &CNetworkDebugPlayback::m_ShowDebugText);

            bank->PushGroup("Playback Ped Data", false);
                bank->AddToggle("Show Local Ped",              &CNetworkDebugPlayback::m_PedPlaybackData.m_ShowLocalPed);
                bank->AddToggle("Show Clone Ped",              &CNetworkDebugPlayback::m_PedPlaybackData.m_ShowClonePed);
                bank->AddToggle("Render Local Debug Vectors",  &CNetworkDebugPlayback::m_PedPlaybackData.m_RenderLocalDebugVectors);
                bank->AddToggle("Render Clone Debug Vectors",  &CNetworkDebugPlayback::m_PedPlaybackData.m_RenderCloneDebugVectors);

                bank->AddButton("Load Playback File", datCallback(CNetworkDebugPlayback::LoadPedPlaybackFile));
                bank->AddButton("Play",               datCallback(CNetworkDebugPlayback::PlayPedData));
                bank->AddButton("Pause",              datCallback(CNetworkDebugPlayback::PausePedData));
                bank->AddButton("Stop",               datCallback(CNetworkDebugPlayback::StopPedData));
                bank->AddButton("Step Forward",       datCallback(CNetworkDebugPlayback::StepForwardPedData));
                bank->AddButton("Step Backward",      datCallback(CNetworkDebugPlayback::StepBackwardPedData));
            bank->PopGroup();

            bank->PushGroup("Playback Vehicle Data", false);
                bank->AddToggle("Show Local Vehicle",             &CNetworkDebugPlayback::m_VehiclePlaybackData.m_ShowLocalVehicle);
                bank->AddToggle("Show Clone Vehicle",             &CNetworkDebugPlayback::m_VehiclePlaybackData.m_ShowCloneVehicle);
                bank->AddToggle("Render Local Forward Vectors",   &CNetworkDebugPlayback::m_VehiclePlaybackData.m_DisplayLocalForwardVector);
                bank->AddToggle("Render Clone Forward Vectors",   &CNetworkDebugPlayback::m_VehiclePlaybackData.m_DisplayCloneForwardVector);
                bank->AddToggle("Render Local Movement Vectors",  &CNetworkDebugPlayback::m_VehiclePlaybackData.m_DisplayLocalMovementVector);
                bank->AddToggle("Render Clone Movement Vectors",  &CNetworkDebugPlayback::m_VehiclePlaybackData.m_DisplayCloneMovementVector);
                bank->AddToggle("Render Local Velocity Vectors",  &CNetworkDebugPlayback::m_VehiclePlaybackData.m_DisplayLocalVelocityVector);
                bank->AddToggle("Render Clone Velocity Vectors",  &CNetworkDebugPlayback::m_VehiclePlaybackData.m_DisplayCloneVelocityVector);

                bank->AddButton("Load Playback File", datCallback(CNetworkDebugPlayback::LoadVehiclePlaybackFile));
                bank->AddButton("Play",               datCallback(CNetworkDebugPlayback::PlayVehicleData));
                bank->AddButton("Pause",              datCallback(CNetworkDebugPlayback::PauseVehicleData));
                bank->AddButton("Stop",               datCallback(CNetworkDebugPlayback::StopVehicleData));
                bank->AddButton("Step Forward",       datCallback(CNetworkDebugPlayback::StepForwardVehicleData));
                bank->AddButton("Step Backward",      datCallback(CNetworkDebugPlayback::StepBackwardVehicleData));
            bank->PopGroup();
        }
        bank->PopGroup();
    }
}

#endif // __BANK

