// Rage headers
#include "script\wrapper.h"

// Framework headers
#include "fwscript/scriptguid.h"

// Game headers
#include "scene\volume\VolumeManager.h"
#include "scene\volume\VolumeAggregate.h"
#include "script\script.h"
#include "script\script_channel.h"
#include "script\handlers\GameScriptResources.h"

namespace volume_commands
{
    int CommandCreateVolumeBox(const scrVector & VOLUME_SUPPORT_ONLY(scrPosition), const scrVector & VOLUME_SUPPORT_ONLY(scrOrientation), const scrVector & VOLUME_SUPPORT_ONLY(scrScale))
    {
#if VOLUME_SUPPORT
        Matrix34 rot;
        CScriptEulers::MatrixFromEulers(rot,Vector3(scrOrientation)*DtoR,EULER_YXZ);
        Mat34V orient = MATRIX34_TO_MAT34V(rot);

        CVolume* pVolume = g_VolumeManager.CreateVolumeBox(Vec3V(scrPosition), orient.GetMat33(), Vec3V(scrScale));
        CScriptResource_Volume newVS(pVolume);
        return CTheScripts::GetCurrentGtaScriptHandler()->RegisterScriptResourceAndGetRef(newVS);
#else
		scriptAssertf(false, "VOLUME_SUPPORT disabled");
		return -1;
#endif // VOLUME_SUPPORT
    }

    int CommandCreateVolumeCylinder(const scrVector & VOLUME_SUPPORT_ONLY(scrPosition), const scrVector & VOLUME_SUPPORT_ONLY(scrOrientation), const scrVector & VOLUME_SUPPORT_ONLY(scrScale))
    {
#if VOLUME_SUPPORT
        Matrix34 rot;
        CScriptEulers::MatrixFromEulers(rot,Vector3(scrOrientation)*DtoR,EULER_YXZ);
        Mat34V orient = MATRIX34_TO_MAT34V(rot);

        CVolume* pVolume = g_VolumeManager.CreateVolumeCylinder(Vec3V(scrPosition), orient.GetMat33(), Vec3V(scrScale));
        CScriptResource_Volume newVS(pVolume);
        return CTheScripts::GetCurrentGtaScriptHandler()->RegisterScriptResourceAndGetRef(newVS);
#else
		scriptAssertf(false, "VOLUME_SUPPORT disabled");
		return -1;
#endif // VOLUME_SUPPORT
    }

    int CommandCreateVolumeSphere(const scrVector & VOLUME_SUPPORT_ONLY(scrPosition), const scrVector & VOLUME_SUPPORT_ONLY(scrOrientation), const scrVector & VOLUME_SUPPORT_ONLY(scrScale))
    {
#if VOLUME_SUPPORT
        Matrix34 rot;
        CScriptEulers::MatrixFromEulers(rot,Vector3(scrOrientation)*DtoR,EULER_YXZ);
        Mat34V orient = MATRIX34_TO_MAT34V(rot);

        CVolume* pVolume = g_VolumeManager.CreateVolumeSphere(Vec3V(scrPosition), orient.GetMat33(), Vec3V(scrScale));
        CScriptResource_Volume newVS(pVolume);
        return CTheScripts::GetCurrentGtaScriptHandler()->RegisterScriptResourceAndGetRef(newVS);
#else
		scriptAssertf(false, "VOLUME_SUPPORT disabled");
		return -1;
#endif // VOLUME_SUPPORT
    }

    int CommandCreateVolumeEllipse(const scrVector & VOLUME_SUPPORT_ONLY(scrPosition), const scrVector & VOLUME_SUPPORT_ONLY(scrOrientation), const scrVector & VOLUME_SUPPORT_ONLY(scrScale))
    {
#if VOLUME_SUPPORT
        Matrix34 rot;
        CScriptEulers::MatrixFromEulers(rot,Vector3(scrOrientation)*DtoR,EULER_YXZ);
        Mat34V orient = MATRIX34_TO_MAT34V(rot);

        CVolume* pVolume = g_VolumeManager.CreateVolumeEllipse(Vec3V(scrPosition), orient.GetMat33(), Vec3V(scrScale));
        CScriptResource_Volume newVS(pVolume);
        return CTheScripts::GetCurrentGtaScriptHandler()->RegisterScriptResourceAndGetRef(newVS);
#else
		scriptAssertf(false, "VOLUME_SUPPORT disabled");
		return -1;
#endif // VOLUME_SUPPORT
    }

    int CommandCreateVolumeAggregate()
    {
#if VOLUME_SUPPORT
        CVolume* pVolume = g_VolumeManager.CreateVolumeAggregate();
        CScriptResource_Volume newVS(pVolume);
        return CTheScripts::GetCurrentGtaScriptHandler()->RegisterScriptResourceAndGetRef(newVS);
#else
		scriptAssertf(false, "VOLUME_SUPPORT disabled");
		return -1;
#endif // VOLUME_SUPPORT
    }

    void CommandAddToVolumeAggregate(int VOLUME_SUPPORT_ONLY(AggregateGUID), int VOLUME_SUPPORT_ONLY(VolumeGUID))
    {
#if VOLUME_SUPPORT
        CVolumeAggregate *pAggregate = fwScriptGuid::FromGuid<CVolumeAggregate>(AggregateGUID);
        CVolume *pVolume = fwScriptGuid::FromGuid<CVolume>(VolumeGUID);

        if (!pAggregate)
        {
            Assertf(pAggregate, "CommandAddToVolumeAggregate :: Volume Aggregate with GUID %d is not valid", AggregateGUID);
            return;
        }

        if (!pVolume)
        {
            Assertf(pVolume, "CommandAddToVolumeAggregate :: Volume with GUID %d is not valid", VolumeGUID);
            return;
        }

        pAggregate->Add(pVolume);
#else
		scriptAssertf(false, "VOLUME_SUPPORT disabled");
#endif // VOLUME_SUPPORT
    }

    void CommandDeleteVolume(int VOLUME_SUPPORT_ONLY(VolumeGUID))
    {
#if VOLUME_SUPPORT
        CTheScripts::GetCurrentGtaScriptHandler()->RemoveScriptResource(CGameScriptResource::SCRIPT_RESOURCE_VOLUME, VolumeGUID);
#else
		scriptAssertf(false, "VOLUME_SUPPORT disabled");
#endif // VOLUME_SUPPORT
    }

    bool CommandDoesVolumeExist(int VOLUME_SUPPORT_ONLY(VolumeGUID))
    {
#if VOLUME_SUPPORT
        CVolume *pVolume = fwScriptGuid::FromGuid<CVolume>(VolumeGUID);

        bool retval = false;
        if (pVolume)
        {
            retval = true;
        }
        return retval;
#else
		scriptAssertf(false, "VOLUME_SUPPORT disabled");
		return false;
#endif // VOLUME_SUPPORT
    }

    bool CommandIsPointInVolume(int VOLUME_SUPPORT_ONLY(VolumeGUID), const scrVector & VOLUME_SUPPORT_ONLY(scrPoint))
    {
#if VOLUME_SUPPORT
        CVolume *pVolume = fwScriptGuid::FromGuid<CVolume>(VolumeGUID);

        if (pVolume)
        {
            return pVolume->IsPointInside(Vec3V(scrPoint));
        }
        Assertf(0, "Volume not found");
		return false;
#else
		scriptAssertf(false, "VOLUME_SUPPORT disabled");
		return false;
#endif // VOLUME_SUPPORT
    }

	scrVector CommandGetVolumeCoords(int VOLUME_SUPPORT_ONLY(VolumeGUID))
	{
#if VOLUME_SUPPORT
		CVolume *pVolume = fwScriptGuid::FromGuid<CVolume>(VolumeGUID);

		Vec3V Pos;
		if (pVolume && pVolume->GetPosition(Pos))
		{
			return scrVector(VEC3V_TO_VECTOR3(Pos));
		}
		Assertf(0, "Volumes not found");
		return scrVector();
#else
		scriptAssertf(false, "VOLUME_SUPPORT disabled");
		return scrVector();
#endif // VOLUME_SUPPORT

	}

	bool CommandSetVolumeCoords(int VOLUME_SUPPORT_ONLY(VolumeGUID), const scrVector & VOLUME_SUPPORT_ONLY(scrPosition))
	{
#if VOLUME_SUPPORT
		CVolume *pVolume = fwScriptGuid::FromGuid<CVolume>(VolumeGUID);

		if (pVolume)
		{
			return pVolume->SetPosition(Vec3V(scrPosition));
		}
		Assertf(0, "Volume not found");
		return false;
#else
		scriptAssertf(false, "VOLUME_SUPPORT disabled");
		return false;
#endif // VOLUME_SUPPORT
	}

	scrVector CommandGetVolumeRotation(int VOLUME_SUPPORT_ONLY(VolumeGUID))
	{
#if VOLUME_SUPPORT
		CVolume *pVolume = fwScriptGuid::FromGuid<CVolume>(VolumeGUID);

		Mat33V rotMat;
		if (pVolume && pVolume->GetOrientation(rotMat))
		{
			Matrix34 mat34 = MAT34V_TO_MATRIX34(Mat34V(rotMat));
			Vector3 eulers = CScriptEulers::MatrixToEulers(mat34, EULER_YXZ);
			return scrVector(eulers*RtoD);
		}
		Assertf(0, "Volume not found");
		return scrVector();
#else
		scriptAssertf(false, "VOLUME_SUPPORT disabled");
		return scrVector();
#endif // VOLUME_SUPPORT
	}

	bool CommandSetVolumeRotation(int VOLUME_SUPPORT_ONLY(VolumeGUID), const scrVector & VOLUME_SUPPORT_ONLY(scrOrientation))
	{
#if VOLUME_SUPPORT
		CVolume *pVolume = fwScriptGuid::FromGuid<CVolume>(VolumeGUID);

		if (pVolume)
		{
			Matrix34 rot;
			CScriptEulers::MatrixFromEulers(rot,Vector3(scrOrientation)*DtoR,EULER_YXZ);
			Mat34V orient = MATRIX34_TO_MAT34V(rot);
			return pVolume->SetOrientation(orient.GetMat33());
		}
		Assertf(0, "Volume not found");
		return false;
#else
		scriptAssertf(false, "VOLUME_SUPPORT disabled");
		return false;
#endif // VOLUME_SUPPORT
	}

	scrVector CommandGetVolumeScale(int VOLUME_SUPPORT_ONLY(VolumeGUID))
	{
#if VOLUME_SUPPORT
		CVolume *pVolume = fwScriptGuid::FromGuid<CVolume>(VolumeGUID);

		Vec3V scale;
		if (pVolume && pVolume->GetScale(scale))
		{
			return scrVector(VEC3V_TO_VECTOR3(scale));
		}
		Assertf(0, "Volume not found");
		return scrVector();
#else
		scriptAssertf(false, "VOLUME_SUPPORT disabled");
		return scrVector();
#endif // VOLUME_SUPPORT
	}

	bool CommandSetVolumeScale(int VOLUME_SUPPORT_ONLY(VolumeGUID), const scrVector & VOLUME_SUPPORT_ONLY(scrScale))
	{
#if VOLUME_SUPPORT
		CVolume *pVolume = fwScriptGuid::FromGuid<CVolume>(VolumeGUID);

		if (pVolume)
		{
			return pVolume->SetScale(Vec3V(scrScale));
		}
		Assertf(0, "Volume not found");
		return false;
#else
		scriptAssertf(false, "VOLUME_SUPPORT disabled");
		return false;
#endif // VOLUME_SUPPORT
	}

    void SetupScriptCommands()
	{
		SCR_REGISTER_UNUSED(CREATE_VOLUME_BOX,0x4b445963518fc6e6, CommandCreateVolumeBox);
        SCR_REGISTER_UNUSED(CREATE_VOLUME_CYLINDER,0x52f47e58d5b88366, CommandCreateVolumeCylinder);
        SCR_REGISTER_UNUSED(CREATE_VOLUME_SPHERE,0x5f828c797a006fe9, CommandCreateVolumeSphere);
        SCR_REGISTER_UNUSED(CREATE_VOLUME_ELLIPSE,0x0fa14262dfc6b79c, CommandCreateVolumeEllipse);
        SCR_REGISTER_UNUSED(CREATE_VOLUME_AGGREGATE,0x49664a4843a6ccda, CommandCreateVolumeAggregate);
        SCR_REGISTER_UNUSED(ADD_TO_VOLUME_AGGREGATE,0xc7207519e6a1a705, CommandAddToVolumeAggregate);
        SCR_REGISTER_UNUSED(DELETE_VOLUME,0x4f238171ea22707e, CommandDeleteVolume);
        SCR_REGISTER_UNUSED(DOES_VOLUME_EXIST,0x2fa4d680f32e3bb3, CommandDoesVolumeExist);
        SCR_REGISTER_UNUSED(IS_POINT_IN_VOLUME,0x063379b7bdf5cca4, CommandIsPointInVolume);
		SCR_REGISTER_UNUSED(GET_VOLUME_COORDS,0x91a12afc1f422493, CommandGetVolumeCoords);
		SCR_REGISTER_UNUSED(SET_VOLUME_COORDS,0xd9e2f4071712238e, CommandSetVolumeCoords);
		SCR_REGISTER_UNUSED(GET_VOLUME_ROTATION,0xdad7bb2b2600a0de, CommandGetVolumeRotation);
		SCR_REGISTER_UNUSED(SET_VOLUME_ROTATION,0xe70fd56034c83524, CommandSetVolumeRotation);
		SCR_REGISTER_UNUSED(GET_VOLUME_SCALE,0xe5925b01a06678a3, CommandGetVolumeScale);
		SCR_REGISTER_UNUSED(SET_VOLUME_SCALE,0x8e8d1ad6bc691874, CommandSetVolumeScale);
	}
}
