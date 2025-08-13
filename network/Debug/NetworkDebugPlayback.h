//
// name:        NetworkDebugPlayback.h
// description: Allows loading of an external file to playback game entity motion in-game
// written by:  Daniel Yelland
//

#ifndef NETWORK_DEBUGPLAYBACK_H
#define NETWORK_DEBUGPLAYBACK_H

#if __BANK

#include "entity/archetypemanager.h"
#include "scene/RegdRefTypes.h"
#include "vector/vector3.h"

class CPed;
class CVehicle;

class CNetworkDebugPlayback
{
public:

    static void Init();
    static void Shutdown();
    static void Update();

    // ped playback
    static void LoadPedPlaybackFile();
    static void PlayPedData();
    static void PausePedData();
    static void StopPedData();
    static void StepForwardPedData();
    static void StepBackwardPedData();

    // vehicle playback
    static void LoadVehiclePlaybackFile();
    static void PlayVehicleData();
    static void PauseVehicleData();
    static void StopVehicleData();
    static void StepForwardVehicleData();
    static void StepBackwardVehicleData();

    //PURPOSE
    // Adds debug widgets for testing the system
    static void AddDebugWidgets();

private:

    struct PedData
    {
        struct Snapshot
        {
            Vector3 m_Position;
            float   m_Heading;
        };

        PedData()
        {
            m_LocalPed                = 0;
            m_ClonePed                = 0;
            m_ShowLocalPed            = true;
            m_ShowClonePed            = true;
            m_RenderLocalDebugVectors = false;
            m_RenderCloneDebugVectors = false;
            m_Playing                 = false;
            m_CurrentFrame            = 0;

            ResetSnapshotData();
        }

        static const unsigned MAX_PED_SNAPSHOTS = 100;

        void AddSnapshot(const Vector3 &localPos, float localHeading,
                         const Vector3 &clonePos, float cloneHeading)
        {
            if(m_NumSnapshots < MAX_PED_SNAPSHOTS)
            {
                m_LocalSnapshots[m_NumSnapshots].m_Position = localPos;
                m_LocalSnapshots[m_NumSnapshots].m_Heading  = localHeading;
                m_CloneSnapshots[m_NumSnapshots].m_Position = clonePos;
                m_CloneSnapshots[m_NumSnapshots].m_Heading  = cloneHeading;
                m_NumSnapshots++;
            }
        }

        void ResetSnapshotData()
        {
            m_NumSnapshots = 0;
        }

        RegdPed  m_LocalPed;
        RegdPed  m_ClonePed;
        bool     m_ShowLocalPed;
        bool     m_ShowClonePed;
        bool     m_RenderLocalDebugVectors;
        bool     m_RenderCloneDebugVectors;
        bool     m_Playing;
        unsigned m_CurrentFrame;
        unsigned m_NumSnapshots;
        Snapshot m_LocalSnapshots[MAX_PED_SNAPSHOTS];
        Snapshot m_CloneSnapshots[MAX_PED_SNAPSHOTS];
    };

    struct VehicleData
    {
        struct Snapshot
        {
            Vector3  m_Position;
            Vector3  m_Velocity;
            Matrix34 m_Orientation;
        };

        VehicleData()
        {
            m_LocalVehicle               = 0;
            m_CloneVehicle               = 0;
            m_ShowLocalVehicle           = true;
            m_ShowCloneVehicle           = true;
            m_DisplayLocalForwardVector  = false;
            m_DisplayCloneForwardVector  = false;
            m_DisplayLocalVelocityVector = false;
            m_DisplayCloneVelocityVector = false;
            m_DisplayLocalMovementVector = false;
            m_DisplayCloneMovementVector = false;
            ResetSnapshotData();
        }

        static const unsigned MAX_VEHICLE_SNAPSHOTS = 400;

        void AddSnapshot(const Vector3 &localPos, const Vector3 &localVel, const Vector3 &localEulers,
                         const Vector3 &clonePos, const Vector3 &cloneVel, const Vector3 &cloneEulers)
        {
            if(m_NumSnapshots < MAX_VEHICLE_SNAPSHOTS)
            {
                m_LocalSnapshots[m_NumSnapshots].m_Position    = localPos;
                m_LocalSnapshots[m_NumSnapshots].m_Velocity    = localVel;
                m_LocalSnapshots[m_NumSnapshots].m_Orientation.FromEulersXYZ(localEulers);

                m_CloneSnapshots[m_NumSnapshots].m_Position    = clonePos;
                m_CloneSnapshots[m_NumSnapshots].m_Velocity    = cloneVel;
                m_CloneSnapshots[m_NumSnapshots].m_Orientation.FromEulersXYZ(cloneEulers);

                m_NumSnapshots++;
            }
        }

        void ResetSnapshotData()
        {
            m_NumSnapshots = 0;
            m_Playing      = false;
            m_CurrentFrame = 0;
        }

        RegdVeh   m_LocalVehicle;               // debug representation of the local vehicle
        RegdVeh   m_CloneVehicle;               // debug representation of the clone vehicle
        bool      m_ShowLocalVehicle;           // indicates the debug representation of the local vehicle should be rendered
        bool      m_ShowCloneVehicle;           // indicates the debug representation of the clone vehicle should be rendered
        bool      m_DisplayLocalForwardVector;  // forward vector at current snapshot (basic orientation debugging)
        bool      m_DisplayCloneForwardVector;  // forward vector at current snapshot (basic orientation debugging)
        bool      m_DisplayLocalVelocityVector; // velocity at current snapshot
        bool      m_DisplayCloneVelocityVector; // velocity at current snapshot
        bool      m_DisplayLocalMovementVector; // vector showing direction of movement between the current and last snapshot
        bool      m_DisplayCloneMovementVector; // vector showing direction of movement between the current and last snapshot
        bool      m_Playing;                    // indicates whether we are playing back the currently loaded snapshot data
        unsigned  m_CurrentFrame;               // the current playback frame
        unsigned  m_NumSnapshots;               // number of snapshots
        Snapshot  m_LocalSnapshots[MAX_VEHICLE_SNAPSHOTS];
        Snapshot  m_CloneSnapshots[MAX_VEHICLE_SNAPSHOTS];
    };

    //PURPOSE
    // Updates the objects and places them in their current positions
    static void UpdateObjects();

    //PURPOSE
    // Creates a ped used to represent the playback in game
    static CPed *CreatePed(fwModelId modelId, const Vector3 &position, const float heading);

    //PURPOSE
    // Creates a vehicle used to represent the playback in game
    static CVehicle *CreateVehicle(fwModelId modelId, const Vector3 &position, const Matrix34 &localOrientation);

    static void DisplayDebugText();

    static PedData     m_PedPlaybackData;
    static VehicleData m_VehiclePlaybackData;
    static bool        m_ShowDebugText;
};

#endif // __BANK

#endif  // NETWORK_DEBUGPLAYBACK_H
