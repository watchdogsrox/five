/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    control/GarageEditor.cpp
// PURPOSE : debug tools to allow management and editing of garages
// AUTHOR :  Flavius Alecu
// CREATED : 17/11/2011
//
/////////////////////////////////////////////////////////////////////////////////

#include "GarageEditor.h"
#include "Garages.h"
#include "garages_parser.h"
#include "objects/Door.h"
#include "grcore\debugdraw.h"
#include "grcore\setup.h"

AI_OPTIMISATIONS()

#if __BANK

#include "camera/CamInterface.h"
#include "grcore/debugdraw.h"
#include "fwscene/world/WorldLimits.h"
#include "peds/ped.h"
#include "script/script.h"
#include "game/ModelIndices.h"

bool CGarageEditor::ms_bDisplayAllGarages = false;
bool CGarageEditor::ms_bDisplayBoundingBoxes = false;
float CGarageEditor::ms_baseX = 0.f;
float CGarageEditor::ms_baseY = 0.f;
float CGarageEditor::ms_baseZ = 0.f;
float CGarageEditor::ms_delta1X = 0.f;
float CGarageEditor::ms_delta1Y = 0.f;
float CGarageEditor::ms_delta2X = 0.f;
float CGarageEditor::ms_delta2Y = 0.f;
float CGarageEditor::ms_ceilingZ = 0.f;
s32 CGarageEditor::ms_type = -1;
int CGarageEditor::ms_permittedVehicleType = 0;
bool CGarageEditor::ms_startWithVehicleSavingEnable = true;
bool CGarageEditor::ms_vehicleSavingEnable = true;
bool CGarageEditor::ms_useLineIntersection= false;
CUiGadgetSimpleListAndWindow* CGarageEditor::ms_pBoxListWindow = NULL;
bool CGarageEditor::ms_bDisplayGarageDebugInfo = false;
bool CGarageEditor::ms_bDisplayBoundingVolumePoints = false;
bool CGarageEditor::ms_drawClearObjectsSphere = false;
bool CGarageEditor::ms_drawCentreSphere = false;
int CGarageEditor::ms_selectedBox = 0;

char CGarageEditor::ms_achCurrentGarageName[GARAGE_NAME_MAX];
bool CGarageEditor::ms_debugTestPeds = true;
bool CGarageEditor::ms_debugTestVehicles = true;
bool CGarageEditor::ms_debugTestObjects = true;

int	CGarageEditor::ms_OwnerSelection = 0;

u8	CGarageEditor::ms_SelectedBoxPulse = 0;

bool CGarageEditor::ms_bEnclosedGarageFlag = false;
bool CGarageEditor::ms_bMPGarageFlag = false;
bool CGarageEditor::ms_bExteriorFlag = false;
bool CGarageEditor::ms_bInteriorFlag = false;

bool CGarageEditor::ms_drawSmallSpheres = false;
bool CGarageEditor::ms_drawLargeSpheres = true;
bool CGarageEditor::ms_drawRectLines = true;
bool CGarageEditor::ms_drawBox2DTest = false;

namespace object_commands
{
	bool CommandIsSavingEnableForGarage(int garageHash);
	void CommandEnableSavingInGarage(int garageHash, bool enable);
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	InitWidgets
// PURPOSE:		creates widgets for editing / displaying garages
//////////////////////////////////////////////////////////////////////////
void CGarageEditor::InitWidgets(bkBank& bank)
{
	bank.PushGroup("Garage Editor");
	bank.AddToggle("Draw garages", &ms_bDisplayAllGarages, datCallback(UiGadgetsCB));
	bank.AddToggle("Draw garage bboxes", &ms_bDisplayBoundingBoxes, datCallback(UiGadgetsCB));
	bank.AddButton("Create garage at camera pos", datCallback(CreateGarageAtCameraPosCB));
	bank.AddButton("Delete current garage", datCallback(DeleteCurrentGarageCB));
	bank.AddSeparator();
	bank.AddToggle("Draw garage Debug Info", &ms_bDisplayGarageDebugInfo);
	bank.AddToggle("Draw player debug visualisations", &ms_bDisplayBoundingVolumePoints);

	bank.AddToggle("Draw player Quad Test points", &ms_drawSmallSpheres);
	bank.AddToggle("Draw player Box Test points", &ms_drawLargeSpheres);
	bank.AddToggle("Draw player Intersection Quad", &ms_drawRectLines);
	bank.AddToggle("Draw Active Box Intersection Quad", &ms_drawBox2DTest);

	bank.AddToggle("Debug Info - Search Peds", &ms_debugTestPeds);
	bank.AddToggle("Debug Info - Search Vehicle", &ms_debugTestVehicles);
	bank.AddToggle("Debug Info - Search Objects", &ms_debugTestObjects);
	bank.AddSeparator();
	bank.AddToggle("Draw clear objects sphere", &ms_drawClearObjectsSphere);
	bank.AddToggle("Draw centre of garage sphere", &ms_drawCentreSphere);

	bank.AddSeparator();

	bank.AddTitle("Tweak currently selected");
	bank.AddText("Garage name", ms_achCurrentGarageName, 256, datCallback(SetNameCB));

	const char *ownerNames[] = { "Any", "Michael", "Franklin", "Trevor" };
	bank.AddCombo("Owner", &ms_OwnerSelection, NELEM(ownerNames), ownerNames, datCallback(SetOwnerCB), "The Garage Owner");

	bank.AddText("Selected Box", &ms_selectedBox, datCallback(SelectedBoxCallback));

	bank.AddButton("Create Box at camera pos", datCallback(CreateBoxAtCameraPosCB));
	bank.AddButton("Delete current Box", datCallback(DeleteCurrentBoxCB));

	bank.AddSlider("Base X", &ms_baseX, WORLDLIMITS_XMIN, WORLDLIMITS_XMAX, 0.1f, datCallback(ResetCurrentGarageBBCallback));
	bank.AddSlider("Base Y", &ms_baseY, WORLDLIMITS_YMIN, WORLDLIMITS_YMAX, 0.1f, datCallback(ResetCurrentGarageBBCallback));
	bank.AddSlider("Base Z", &ms_baseZ, WORLDLIMITS_ZMIN, WORLDLIMITS_ZMAX, 0.1f, datCallback(ResetCurrentGarageBBCallback));

	bank.AddSlider("Delta 1 X", &ms_delta1X, -50.f, 50.f, 0.1f, datCallback(ResetCurrentGarageBBCallback));
	bank.AddSlider("Delta 1 Y", &ms_delta1Y, -50.f, 50.f, 0.1f, datCallback(ResetCurrentGarageBBCallback));
	bank.AddButton("Snap Delat2 to be perpendicular to Delta1", datCallback(SnapDelta2PerpendicularToDelta1CB));

	bank.AddSlider("Delta 2 X", &ms_delta2X, -50.f, 50.f, 0.1f, datCallback(ResetCurrentGarageBBCallback));
	bank.AddSlider("Delta 2 Y", &ms_delta2Y, -50.f, 50.f, 0.1f, datCallback(ResetCurrentGarageBBCallback));
	bank.AddButton("Snap Delat1 to be perpendicular to Delta2", datCallback(SnapDelta1PerpendicularToDelta2CB));

	bank.AddSlider("Ceiling Z", &ms_ceilingZ, WORLDLIMITS_ZMIN, WORLDLIMITS_ZMAX, 0.1f);

	bank.AddToggle("Is Enclosed Garage", &ms_bEnclosedGarageFlag, datCallback(EnclosedGarageFlagSetCB));
	bank.AddToggle("Is MP Garage", &ms_bMPGarageFlag, datCallback(MPGarageFlagSetCB));
	bank.AddToggle("Is Exterior Box", &ms_bExteriorFlag, datCallback(ExteriorFlagSetCB));
	bank.AddToggle("Is Interior Box", &ms_bInteriorFlag, datCallback(InteriorFlagSetCB));

	const char* garageTypes[] = { "None", "Mission", "Respray", "Script", "Ambient parking", "Safe house" };
	bank.AddCombo("Type", &ms_type, NELEM(garageTypes), &garageTypes[0]);

	const char* vehicleTypes[] = { "Default", "Helicopters", "Boats"};
	bank.AddCombo("Permitted Vehicle Type", &ms_permittedVehicleType, NELEM(vehicleTypes), &vehicleTypes[0]);
	bank.AddToggle("Start with Vehicle Saving Enabled", &ms_startWithVehicleSavingEnable);
	bank.AddToggle("Vehicle Saving Enabled", &ms_vehicleSavingEnable);
	bank.AddToggle("Use line intersection for Partially inside test", &ms_useLineIntersection);

	bank.AddButton("Clear selected garage", datCallback(ClearSelectedGarageCB));
	bank.AddButton("Clear selected garage of Peds", datCallback(ClearSelectedGarageOfPedsCB));
	bank.AddButton("Clear selected garage of Vehicles", datCallback(ClearSelectedGarageOfVehiclesCB));
	bank.AddButton("Clear selected garage of objects", datCallback(ClearSelectedGarageOfObjectsCB));
	bank.AddSeparator();

	bank.AddTitle("Save or load common:/data/garages.meta");
	bank.AddButton("Save garages", datCallback(Save));
	bank.AddButton("Reload garages", datCallback(Load));
	bank.PopGroup();
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	UpdateDebug
// PURPOSE:		handles debug draw etc for editing / display cull boxes
//////////////////////////////////////////////////////////////////////////
void CGarageEditor::UpdateDebug()
{
	int garageIndex = ms_pBoxListWindow ? ms_pBoxListWindow->GetCurrentIndex() : -1;
	if (ms_bDisplayAllGarages)
	{
		DisplayGarages();

		if (garageIndex != -1)
		{
			if (ms_pBoxListWindow->UserProcessClick())
			{
				RefreshTweakVals();
			}
			else 
			{
				if (garageIndex >=0 && garageIndex < CGarages::aGarages.GetCount())
				{
					CGarage& g = CGarages::aGarages[ms_pBoxListWindow->GetCurrentIndex()];
					CGarage::Box box;
					
					box.BaseX = ms_baseX;
					box.BaseY = ms_baseY;
					box.BaseZ = ms_baseZ;

					box.Delta1X = ms_delta1X;
					box.Delta1Y = ms_delta1Y;

					box.Delta2X = ms_delta2X;
					box.Delta2Y = ms_delta2Y;

					box.CeilingZ = ms_ceilingZ;

					box.useLineIntersection = ms_useLineIntersection;

					g.UpdateSize(ms_selectedBox, box);
					g.SetType((GarageType)ms_type);

					if (ms_permittedVehicleType == 1)
					{
						g.m_permittedVehicleType = VEHICLE_TYPE_HELI;
					}
					else if (ms_permittedVehicleType == 2)
					{
						g.m_permittedVehicleType = VEHICLE_TYPE_BOAT;
					}
					else
					{
						g.m_permittedVehicleType = VEHICLE_TYPE_NONE;
					}
					g.Flags.bSavingVehiclesWasEnabledBeforeSave = ms_startWithVehicleSavingEnable;
					object_commands::CommandEnableSavingInGarage(g.name.GetHash(), ms_vehicleSavingEnable);
//					g.Flags.bSavingVehiclesEnabled = ms_vehicleSavingEnable;
				}
			}
		}
	}

	if (ms_bDisplayGarageDebugInfo && garageIndex >=0 && garageIndex < CGarages::aGarages.GetCount())
	{
		const float _XPos = 270.0f;
		const float _YPos = 580.0f;
		const float _YInc = 10.0f;

		Vector2 startPosition(_XPos, _YPos);
		if (CGarages::IsGarageEmpty(garageIndex))
		{
			grcDebugDraw::TextFontPush(grcSetup::GetMiniFixedWidthFont());

			const atVarString empty("Garage is Empty");

			grcDebugDraw::Text(startPosition, DD_ePCS_Pixels, CRGBA_White(), empty, true, 1.0f, 1.0f);
			startPosition.y += _YInc;

			grcDebugDraw::TextFontPop();
		}

		const CPed *pPed = FindPlayerPed();
		if (pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ), "GET_VEHICLE_PED_IS_IN - Ped is not in a vehicle")
		{
			CVehicle *pVehicle = pPed->GetMyVehicle();
			if (pVehicle)
			{
				CTheScripts::GetGUIDFromEntity(*pVehicle);
			}
		}
 		if (CGarages::IsPlayerEntirelyOutsideGarage(garageIndex, FindPlayerPed(), 0.0f))
		{
			grcDebugDraw::TextFontPush(grcSetup::GetMiniFixedWidthFont());

			const atVarString outside("Player is outside Garage");

			grcDebugDraw::Text(startPosition, DD_ePCS_Pixels, CRGBA_White(), outside, true, 1.0f, 1.0f);
			startPosition.y += _YInc;

			grcDebugDraw::TextFontPop();
		}

		
		if (CGarages::IsPlayerEntirelyInsideGarage(garageIndex, FindPlayerPed(), 0.0f))
		{
			grcDebugDraw::TextFontPush(grcSetup::GetMiniFixedWidthFont());

			const atVarString inside("Player is inside Garage");

			grcDebugDraw::Text(startPosition, DD_ePCS_Pixels, CRGBA_White(), inside, true, 1.0f, 1.0f);
			startPosition.y += _YInc;

			for (int i = 0; i < CGarages::aGarages[garageIndex].m_boxes.GetCount(); ++i)
			{
				if (CGarages::IsPlayerEntirelyInsideGarage(garageIndex, FindPlayerPed(), 0.0f, i))
				{
					const atVarString inside("Player is inside box %d",i);

					grcDebugDraw::Text(startPosition, DD_ePCS_Pixels, CRGBA_White(), inside, true, 1.0f, 1.0f);
					startPosition.y += _YInc;
				}
			}
			grcDebugDraw::TextFontPop();
		}
		else if (CGarages::IsPlayerPartiallyInsideGarage(garageIndex, FindPlayerPed()))
		{
			grcDebugDraw::TextFontPush(grcSetup::GetMiniFixedWidthFont());

			const atVarString partial("Player is partially inside Garage");

			grcDebugDraw::Text(startPosition, DD_ePCS_Pixels, CRGBA_White(), partial, true, 1.0f, 1.0f);
			startPosition.y += _YInc;

			grcDebugDraw::TextFontPop();
		}
		
		if (garageIndex != -1 && ms_selectedBox < CGarages::aGarages[garageIndex].m_boxes.GetCount())
		{
			if (CGarages::IsPlayerPartiallyInsideGarage(garageIndex, FindPlayerPed(), ms_selectedBox))
			{
				grcDebugDraw::TextFontPush(grcSetup::GetMiniFixedWidthFont());

				const atVarString partial("Player is partially inside box %d", ms_selectedBox);

				grcDebugDraw::Text(startPosition, DD_ePCS_Pixels, CRGBA_White(), partial, true, 1.0f, 1.0f);
				startPosition.y += _YInc;

				grcDebugDraw::TextFontPop();
			}
		}


		atArray<CEntity*> objects;
		CGarages::FindAllObjectsInGarage(garageIndex, objects, ms_debugTestPeds, ms_debugTestVehicles, ms_debugTestObjects);
		if (objects.GetCount() > 0)
		{
			grcDebugDraw::TextFontPush(grcSetup::GetMiniFixedWidthFont());
			const atVarString Heading("Objects Inside Garage");
			grcDebugDraw::Text(startPosition, DD_ePCS_Pixels, CRGBA_White(), Heading, true, 1.0f, 1.0f);
			startPosition.y += _YInc;

			for (int i = 0; i < objects.GetCount(); ++i)
			{
				const atVarString object("%s", objects[i]->GetModelName());
				grcDebugDraw::Text(startPosition, DD_ePCS_Pixels, CRGBA_White(), object, true, 1.0f, 1.0f);
				startPosition.y += _YInc;
			}
			grcDebugDraw::TextFontPop();
		}
		if (CGarages::IsAnyEntityEntirelyInsideGarage(garageIndex, ms_debugTestPeds, ms_debugTestVehicles, ms_debugTestObjects, 0.0f))
		{
			grcDebugDraw::TextFontPush(grcSetup::GetMiniFixedWidthFont());

			const atVarString partial("IsAnyEntityEntirelyInsideGarage - Yes");

			grcDebugDraw::Text(startPosition, DD_ePCS_Pixels, CRGBA_White(), partial, true, 1.0f, 1.0f);
			startPosition.y += _YInc;

			grcDebugDraw::TextFontPop();
		}
		if (CGarages::aGarages[garageIndex].type == GARAGE_SAFEHOUSE_PARKING_ZONE)
		{
			grcDebugDraw::TextFontPush(grcSetup::GetMiniFixedWidthFont());

			u32 hash = CGarages::aGarages[garageIndex].name.GetHash();
			bool enabled = object_commands::CommandIsSavingEnableForGarage(hash);
			const atVarString partial("Saving of Vehicles Enabled - %s", enabled /*CGarages::aGarages[garageIndex].Flags.bSavingVehiclesEnabled */? "Yes" : "No");

			grcDebugDraw::Text(startPosition, DD_ePCS_Pixels, CRGBA_White(), partial, true, 1.0f, 1.0f);
			startPosition.y += _YInc;

			grcDebugDraw::TextFontPop();
		}

		// Is Enclosed Garage
		grcDebugDraw::TextFontPush(grcSetup::GetMiniFixedWidthFont());
		{
			const atVarString partial("Is Enclosed Garage - %s", CGarages::aGarages[garageIndex].m_IsEnclosed ? "Yes" : "No");
			grcDebugDraw::Text(startPosition, DD_ePCS_Pixels, CRGBA_White(), partial, true, 1.0f, 1.0f);
			startPosition.y += _YInc;
		}
		// Is MP Garage
		{
			const atVarString partial("Is MP Garage - %s", CGarages::aGarages[garageIndex].m_IsMPGarage ? "Yes" : "No");
			grcDebugDraw::Text(startPosition, DD_ePCS_Pixels, CRGBA_White(), partial, true, 1.0f, 1.0f);
			startPosition.y += _YInc;
		}
		grcDebugDraw::TextFontPop();
	}

	if (ms_bDisplayBoundingVolumePoints)
	{
		CPed *pPlayer = FindPlayerPed();
		CEntity *pEntity;
		if (pPlayer->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle))
		{
			pEntity = pPlayer->GetMyVehicle();
		}
		else
		{
			pEntity = pPlayer;
		}

		if (ms_drawSmallSpheres)
		{
			Vector3 p0, p1, p2, p3;
			CGarage::BuildQuadTestPoints(pEntity, p0, p1, p2, p3);

			grcDebugDraw::Sphere(p0, 0.025f, Color32(0xFF,	  0, 0, 0xFF));
			grcDebugDraw::Sphere(p1, 0.025f, Color32(   0, 0xFF, 0, 0xFF));

			grcDebugDraw::Sphere(p2, 0.025f, Color32(0,    0, 0xFF, 0xFF));
			grcDebugDraw::Sphere(p3, 0.025f, Color32(0xFF, 0, 0xFF, 0xFF));
		}

		if (ms_drawLargeSpheres || ms_drawRectLines)
		{
			Vector3 p0, p1, p2, p3, p4, p5, p6, p7;
			CGarage::BuildBoxTestPoints(pEntity, p0, p1, p2, p3, p4, p5, p6, p7);

			if (ms_drawLargeSpheres)
			{
				grcDebugDraw::Sphere(p0, 0.05f, Color32(0xFF,	  0, 0, 0xFF));
				grcDebugDraw::Sphere(p1, 0.05f, Color32(   0, 0xFF, 0, 0xFF));

				grcDebugDraw::Sphere(p2, 0.05f, Color32(0,    0, 0xFF, 0xFF));
				grcDebugDraw::Sphere(p3, 0.05f, Color32(0xFF, 0, 0xFF, 0xFF));

				grcDebugDraw::Sphere(p4, 0.05f, Color32(0x80,    0, 0, 0xFF));
				grcDebugDraw::Sphere(p5, 0.05f, Color32(   0, 0x80, 0, 0xFF));

				grcDebugDraw::Sphere(p6, 0.05f, Color32(0,    0, 0x80, 0xFF));
				grcDebugDraw::Sphere(p7, 0.05f, Color32(0x80, 0, 0x80, 0xFF));

			}
			if (ms_drawRectLines)
			{
				Vector3 pA(p0.x, p0.y, (p0.z + p1.z)* 0.5f);
				Vector3 pB(p5.x, p5.y, (p5.z + p6.z)* 0.5f);
				Vector3 pC(p3.x, p3.y, (p3.z + p2.z)* 0.5f);
				Vector3 pD(p4.x, p4.y, (p4.z + p7.z)* 0.5f);

				grcDebugDraw::Sphere(pA, 0.025f, Color32(0xFF,	  0, 0, 0x80));
				grcDebugDraw::Sphere(pB, 0.025f, Color32(   0, 0xFF, 0, 0x80));

				grcDebugDraw::Sphere(pC, 0.025f, Color32(0,    0, 0xFF, 0x80));
				grcDebugDraw::Sphere(pD, 0.025f, Color32(0xFF, 0, 0xFF, 0x80));

				Color32 colWhite = Color32(0xFF, 0xFF, 0xFF, 0xFF);
				Color32 colRed   = Color32(0xFF, 0, 0, 0xFF);

				Color32 color = colWhite;
				if (CGarages::IsLineIntersectingGarage(garageIndex, pA, pB, ms_selectedBox))
				{
					color = colRed;
				}
				grcDebugDraw::Line(pA, pB, color); 

				color = colWhite;
				if (CGarages::IsLineIntersectingGarage(garageIndex, pC, pD, ms_selectedBox))
				{
					color = colRed;
				}
				grcDebugDraw::Line(pC, pD, color); 

				color = colWhite;
				if (CGarages::IsLineIntersectingGarage(garageIndex, pA, pC, ms_selectedBox))
				{
					color = colRed;
				}
				grcDebugDraw::Line(pA, pC, color); 

				color = colWhite;
				if (CGarages::IsLineIntersectingGarage(garageIndex, pB, pD, ms_selectedBox))
				{
					color = colRed;
				}
				grcDebugDraw::Line(pB, pD, color); 
			}

		}

		if (ms_drawBox2DTest && garageIndex != -1)
		{
			if (garageIndex < CGarages::aGarages.GetCount())
			{
				if (ms_selectedBox < CGarages::aGarages[garageIndex].m_boxes.GetCount())
				{
					CGarage::Box &box = CGarages::aGarages[garageIndex].m_boxes[ms_selectedBox];
					//      p2 ------------ p3
					// 		|				|
					// 		|				|
					// 		|				|
					// 		|				|
					// 		|				|
					//		p0 ------------ p1

					Vector2 p0(box.BaseX, box.BaseY);
					Vector2 p1(p0.x + box.Delta1X, p0.y + box.Delta1Y);
					Vector2 p2(p0.x + box.Delta2X, p0.y + box.Delta2Y);
					Vector2 p3(p1.x + box.Delta2X, p1.y + box.Delta2Y);

					float z = (box.BaseZ + box.CeilingZ) * 0.5f;

					// line p0 - p1
					grcDebugDraw::Line(Vector3(p0.x, p0.y, z), Vector3(p1.x, p1.y, z), Color32(0x80, 0, 0, 0xFF)); 

					// line p2 - p3
					grcDebugDraw::Line(Vector3(p2.x, p2.y, z), Vector3(p3.x, p3.y, z), Color32(0, 0x80, 0, 0xFF));

					// line p0 - p2
					grcDebugDraw::Line(Vector3(p0.x, p0.y, z), Vector3(p2.x, p2.y, z), Color32(0, 0, 0x80, 0xFF));

					// line p1 - p3
					grcDebugDraw::Line(Vector3(p1.x, p1.y, z), Vector3(p3.x, p3.y, z), Color32(0x80, 0x80, 0, 0xFF));
				}
			}
			
		}


	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	DisplayGarages
// PURPOSE:		debug draw, display all garages
//////////////////////////////////////////////////////////////////////////
void CGarageEditor::DisplayGarages()
{
	int garageIndex = -1;
	for (s32 i = 0; i < CGarages::aGarages.GetCount(); ++i)
	{
		const CGarage* pG = &CGarages::aGarages[i];	

		for (int j = 0; j < pG->m_boxes.GetCount(); ++j)
		{	
			const CGarage::Box &box = pG->m_boxes[j];
			
			Color32 boxColor(0x80, 0x80, 0x80, 0xFF);
			if (ms_pBoxListWindow)
			{ 
				garageIndex = ms_pBoxListWindow->GetCurrentIndex();
				if (garageIndex == i && ms_selectedBox == j)
				{
					ms_SelectedBoxPulse += 10;

					boxColor = Color32(ms_SelectedBoxPulse, ms_SelectedBoxPulse, ms_SelectedBoxPulse, 0xff);
				}
			}
			if (Vector2(box.BaseX - camInterface::GetPos().x, box.BaseY - camInterface::GetPos().y).Mag() < 100.0f)
			{
				// Lower area
				grcDebugDraw::Line(Vector3(box.BaseX, box.BaseY, box.BaseZ),
					Vector3(box.BaseX + box.Delta1X, box.BaseY + box.Delta1Y, box.BaseZ), 
					boxColor);
				grcDebugDraw::Line(Vector3(box.BaseX + box.Delta1X, box.BaseY + box.Delta1Y, box.BaseZ),
					Vector3(box.BaseX + box.Delta1X + box.Delta2X, box.BaseY + box.Delta1Y + box.Delta2Y, box.BaseZ), 
					boxColor);
				grcDebugDraw::Line(Vector3(box.BaseX, box.BaseY, box.BaseZ),
					Vector3(box.BaseX + box.Delta2X, box.BaseY + box.Delta2Y, box.BaseZ), 
					boxColor);
				grcDebugDraw::Line(Vector3(box.BaseX + box.Delta2X, box.BaseY + box.Delta2Y, box.BaseZ),
					Vector3(box.BaseX + box.Delta1X + box.Delta2X, box.BaseY + box.Delta1Y + box.Delta2Y, box.BaseZ), 
					boxColor);

				// Ceiling area
				grcDebugDraw::Line(Vector3(box.BaseX, box.BaseY, box.CeilingZ),
					Vector3(box.BaseX + box.Delta1X, box.BaseY + box.Delta1Y, box.CeilingZ), 
					boxColor);
				grcDebugDraw::Line(Vector3(box.BaseX + box.Delta1X, box.BaseY + box.Delta1Y, box.CeilingZ),
					Vector3(box.BaseX + box.Delta1X + box.Delta2X, box.BaseY + box.Delta1Y + box.Delta2Y, box.CeilingZ), 
					boxColor);
				grcDebugDraw::Line(Vector3(box.BaseX, box.BaseY, box.CeilingZ),
					Vector3(box.BaseX + box.Delta2X, box.BaseY + box.Delta2Y, box.CeilingZ), 
					boxColor);
				grcDebugDraw::Line(Vector3(box.BaseX + box.Delta2X, box.BaseY + box.Delta2Y, box.CeilingZ),
					Vector3(box.BaseX + box.Delta1X + box.Delta2X, box.BaseY + box.Delta1Y + box.Delta2Y, box.CeilingZ), 
					boxColor);

				// Four pillars
				grcDebugDraw::Line(Vector3(box.BaseX, box.BaseY, box.BaseZ),
					Vector3(box.BaseX, box.BaseY, box.CeilingZ), 
					boxColor);
				grcDebugDraw::Line(Vector3(box.BaseX + box.Delta1X, box.BaseY + box.Delta1Y, box.BaseZ),
					Vector3(box.BaseX + box.Delta1X, box.BaseY + box.Delta1Y, box.CeilingZ), 
					boxColor);
				grcDebugDraw::Line(Vector3(box.BaseX + box.Delta2X, box.BaseY + box.Delta2Y, box.BaseZ),
					Vector3(box.BaseX + box.Delta2X, box.BaseY + box.Delta2Y, box.CeilingZ), 
					boxColor);
				grcDebugDraw::Line(Vector3(box.BaseX + box.Delta1X + box.Delta2X, box.BaseY + box.Delta1Y + box.Delta2Y, box.BaseZ), 
					Vector3(box.BaseX + box.Delta1X + box.Delta2X, box.BaseY + box.Delta1Y + box.Delta2Y, box.CeilingZ), 
					boxColor);
		
				if (garageIndex == i && j == 0 && (ms_drawClearObjectsSphere || ms_drawCentreSphere))
				{
					float z = (box.CeilingZ - box.BaseZ) * 0.5f + box.BaseZ;

					Vector2 delta1(box.Delta1X, box.Delta1Y);
					Vector2 delta2(box.Delta2X, box.Delta2Y);
					Vector2 base(box.BaseX, box.BaseY);

					Vector2 pos2 = base + ((delta1 + delta2) * 0.5f);
					Vector3 position(pos2.x, pos2.y, z);

					float halfMag1 = delta1.Mag() * 0.5f;
					float halfMag2 = delta2.Mag() * 0.5f;
					float radius = Min(halfMag1, halfMag2);

					if (ms_drawCentreSphere)
					{
						grcDebugDraw::Sphere(position, 0.1f, Color32(0xff, 0xff, 0xff, 0x20));
					}
					if (ms_drawClearObjectsSphere)
					{
						grcDebugDraw::Sphere(position, radius, Color32(0xff, 0x0, 0x0, 0x20));
					}
				}


				// Display a crosshair at the doors' coordinate
				if (garageIndex == i && pG->m_pDoorObject)
				{
					//						Vector3	doorPos = VEC3V_TO_VECTOR3(pG->m_pDoorObject->GetTransform().GetPosition());
					Vector3	doorPosMinX = pG->m_pDoorObject->TransformIntoWorldSpace(Vector3(-1.0f, 0.0f, 0.0f));
					Vector3	doorPosMaxX = pG->m_pDoorObject->TransformIntoWorldSpace(Vector3(1.0f, 0.0f, 0.0f));
					Vector3	doorPosMinY = pG->m_pDoorObject->TransformIntoWorldSpace(Vector3(0.0f, -1.0f, 0.0f));
					Vector3	doorPosMaxY = pG->m_pDoorObject->TransformIntoWorldSpace(Vector3(0.0f, 1.0f, 0.0f));
					Vector3	doorPosMinZ = pG->m_pDoorObject->TransformIntoWorldSpace(Vector3(0.0f, 0.0f, -1.0f));
					Vector3	doorPosMaxZ = pG->m_pDoorObject->TransformIntoWorldSpace(Vector3(0.0f, 0.0f, 1.0f));
					grcDebugDraw::Line(doorPosMinX, doorPosMaxX, Color32(0xff, 0x0, 0x0, 0xff));
					grcDebugDraw::Line(doorPosMinY, doorPosMaxY, Color32(0x0, 0xff, 0x0, 0xff));
					grcDebugDraw::Line(doorPosMinZ, doorPosMaxZ, Color32(0x0, 0x0, 0xff, 0xff));
				}

				if (garageIndex == i && j == 0)
				{
					// Display the name; type and state.
					char	debugStr[256];
					sprintf(debugStr, "%s - Type:%d State:%d", pG->name.GetCStr(), pG->type, pG->state);
					grcDebugDraw::Text(Vector3(box.BaseX + 0.5f * box.Delta1X, box.BaseY + 0.5f * box.Delta1Y, (box.BaseZ + box.CeilingZ) * 0.5f), 
						CRGBA(255, 255, 255, 255), 
						debugStr);
				}

				// draw bounding box
				if (ms_bDisplayBoundingBoxes)
				{
					Vec3V bbMin(pG->MinX, pG->MinY, pG->MinZ);
					Vec3V bbMax(pG->MaxX, pG->MaxY, pG->MaxZ);
					grcDebugDraw::BoxAxisAligned(bbMin, bbMax, Color32(255, 0, 0), false);
				}

			}
		}
	}
}
//////////////////////////////////////////////////////////////////////////
// FUNCTION:	CreateGarageAtCameraPosCB
// PURPOSE:		creates a new box at camera position
//////////////////////////////////////////////////////////////////////////

void CGarageEditor::CreateBoxAtCameraPosCB()
{
	if (ms_pBoxListWindow)
	{
		int garageIndex = ms_pBoxListWindow->GetCurrentIndex();
		if (garageIndex >= 0 && garageIndex < CGarages::aGarages.GetCount())
		{
			CGarage &garage = CGarages::aGarages[garageIndex];
			garage.m_boxes.Grow();
			ms_selectedBox = garage.m_boxes.GetCount() - 1;
			const Vector3& vPos = camInterface::GetPos();
			CGarage::Box box;

			box.BaseX = vPos.x;
			box.BaseY = vPos.y;
			box.BaseZ = vPos.z - DEFAULT_GARAGE_HEIGHT / 2.f;
	
			box.Delta1X = DEFAULT_GARAGE_WIDTH;
			box.Delta1Y = 0.0f;

			box.Delta2X = 0.0f;
			box.Delta2Y = DEFAULT_GARAGE_DEPTH;

			box.CeilingZ = vPos.z + DEFAULT_GARAGE_HEIGHT / 2.f;

			ms_bInteriorFlag = false;
			ms_bExteriorFlag = false;

			garage.UpdateSize(ms_selectedBox, box);
			RefreshTweakVals();
		}
	}
}

namespace object_commands 
{
	void CommandClearGarage(int garageHash, bool bBroadcast);
	void CommandClearObjectsFromGarage(int garageHash, bool vehicles, bool peds, bool objects, bool bBroadcast);
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	ClearSelectedGarageCB
//////////////////////////////////////////////////////////////////////////

void CGarageEditor::ClearSelectedGarageCB()
{
	if (ms_pBoxListWindow)
	{
		int garageIndex = ms_pBoxListWindow->GetCurrentIndex();
		if (garageIndex >= 0 && garageIndex < CGarages::aGarages.GetCount())
		{
			CGarage &garage = CGarages::aGarages[garageIndex];
			object_commands::CommandClearGarage(garage.name.GetHash(), false);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	ClearSelectedGarageOfPedsCB
//////////////////////////////////////////////////////////////////////////

void CGarageEditor::ClearSelectedGarageOfPedsCB()
{
	if (ms_pBoxListWindow)
	{
		int garageIndex = ms_pBoxListWindow->GetCurrentIndex();
		if (garageIndex >= 0 && garageIndex < CGarages::aGarages.GetCount())
		{
			CGarage &garage = CGarages::aGarages[garageIndex];
			object_commands::CommandClearObjectsFromGarage(garage.name.GetHash(), false, true, false, false);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	ClearSelectedGarageOfVehiclesCB
//////////////////////////////////////////////////////////////////////////

void CGarageEditor::ClearSelectedGarageOfVehiclesCB()
{
	if (ms_pBoxListWindow)
	{
		int garageIndex = ms_pBoxListWindow->GetCurrentIndex();
		if (garageIndex >= 0 && garageIndex < CGarages::aGarages.GetCount())
		{
			CGarage &garage = CGarages::aGarages[garageIndex];
			object_commands::CommandClearObjectsFromGarage(garage.name.GetHash(), true, false, false, false);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	ClearSelectedGarageOfObjectsCB
//////////////////////////////////////////////////////////////////////////

void CGarageEditor::ClearSelectedGarageOfObjectsCB()
{
	if (ms_pBoxListWindow)
	{
		int garageIndex = ms_pBoxListWindow->GetCurrentIndex();
		if (garageIndex >= 0 && garageIndex < CGarages::aGarages.GetCount())
		{
			CGarage &garage = CGarages::aGarages[garageIndex];
			object_commands::CommandClearObjectsFromGarage(garage.name.GetHash(), false, false, true, false);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	DeleteCurrentGarageCB
// PURPOSE:		deletes the currently selected box
//////////////////////////////////////////////////////////////////////////

void CGarageEditor::DeleteCurrentBoxCB()
{
	if (ms_pBoxListWindow && ms_pBoxListWindow->GetCurrentIndex() != -1)
	{
		int garageIndex = ms_pBoxListWindow->GetCurrentIndex();
		if (garageIndex >= 0 && garageIndex < CGarages::aGarages.GetCount())
		{
			CGarage &garage = CGarages::aGarages[garageIndex];
			if (garage.m_boxes.GetCount() > 1)
			{
				garage.m_boxes.Delete(ms_selectedBox);
				--ms_selectedBox;
				ms_selectedBox = MAX(0, ms_selectedBox);
				RefreshTweakVals();
				ResetCurrentGarageBBCallback();
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	SelectedBoxCallback
// PURPOSE:		Ensure selected box index is valid
//////////////////////////////////////////////////////////////////////////
void CGarageEditor::SelectedBoxCallback()
{
	if (ms_pBoxListWindow)
	{ 
		int garageIndex = ms_pBoxListWindow->GetCurrentIndex();
		if (garageIndex >= 0 && garageIndex < CGarages::aGarages.GetCount())
		{
			CGarage &garage = CGarages::aGarages[garageIndex];
			if (ms_selectedBox >= garage.m_boxes.GetCount())
			{
				ms_selectedBox = garage.m_boxes.GetCount() - 1;
			}
			else if (ms_selectedBox <  0)
			{
				ms_selectedBox = 0;
			}
			RefreshTweakVals();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	ResetCurrentGarageBBCallback
// PURPOSE:		
//////////////////////////////////////////////////////////////////////////
void CGarageEditor::ResetCurrentGarageBBCallback()
{
	int garageIndex = ms_pBoxListWindow->GetCurrentIndex();
	if (garageIndex >= 0 && garageIndex < CGarages::aGarages.GetCount())
	{
		CGarage &garage = CGarages::aGarages[garageIndex];
		garage.MinX = FLT_MAX; 
		garage.MaxX = -FLT_MAX; 

		garage.MinY = FLT_MAX;
		garage.MaxY = -FLT_MAX;

		garage.MinZ = FLT_MAX;
		garage.MaxZ = -FLT_MAX;
		for (int i = 0; i < garage.m_boxes.GetCount(); ++i)
		{
			garage.UpdateSize(i, garage.m_boxes[i]);
		}
	}	
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	SnapVector2PerpendicularToVector1
// PURPOSE:		
//////////////////////////////////////////////////////////////////////////
void SnapVector2PerpendicularToVector1(float v1x, float v1y, float *pV2x, float *PV2y)
{
	Vector2 d1(v1x, v1y);
	Vector2 d2(*pV2x, *PV2y);
	float magD2 = d2.Mag();
	d1.Normalize();
	d2.Normalize();

	Vector2 result(d1.y, -d1.x);

	float dot = result.Dot(d2);

	if (dot < 0.0f)
	{
		result.x = -d1.y;
		result.y =  d1.x;
	}

	result *= magD2;
	*pV2x = result.x;
	*PV2y = result.y;
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	SnapDelta2PerpendicularToDelta1CB
// PURPOSE:		
//////////////////////////////////////////////////////////////////////////
void CGarageEditor::SnapDelta2PerpendicularToDelta1CB()
{
	SnapVector2PerpendicularToVector1(ms_delta1X, ms_delta1Y, &ms_delta2X, &ms_delta2Y);
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	SnapDelta1PerpendicularToDelta2VB
// PURPOSE:		
//////////////////////////////////////////////////////////////////////////
void CGarageEditor::SnapDelta1PerpendicularToDelta2CB()
{
	SnapVector2PerpendicularToVector1(ms_delta2X, ms_delta2Y, &ms_delta1X, &ms_delta1Y);
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	UiGadgetsCB
// PURPOSE:		construct / destroy any debug ui elements required
//////////////////////////////////////////////////////////////////////////
void CGarageEditor::UiGadgetsCB()
{
	if (ms_bDisplayAllGarages)
	{
		if (!ms_pBoxListWindow)
		{
			ms_pBoxListWindow = rage_new CUiGadgetSimpleListAndWindow("Garages", 700.0f, 60.0f, 150.0f, 40);
			ms_pBoxListWindow->SetNumEntries(CGarages::aGarages.GetCount());
			ms_pBoxListWindow->SetUpdateCellCB(UpdateBoxCellCB);
		}
	}
	else
	{
		if (ms_pBoxListWindow)
		{
			delete ms_pBoxListWindow;
			ms_pBoxListWindow = NULL;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	UpdateBoxCellCB
// PURPOSE:		callback to set cell contents for garage list
//////////////////////////////////////////////////////////////////////////

void CGarageEditor::UpdateBoxCellCB(CUiGadgetText* pResult, u32 row, u32 UNUSED_PARAM(col), void* UNUSED_PARAM(extraCallbackData) )
{
	if (row < CGarages::aGarages.GetCount())
	{
		pResult->SetString(CGarages::aGarages[row].name.GetCStr());
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	CreateGarageAtCameraPosCB
// PURPOSE:		creates a new garage at camera position
//////////////////////////////////////////////////////////////////////////
void CGarageEditor::CreateGarageAtCameraPosCB()
{
	if (ms_pBoxListWindow)
	{
		static int newGarageIndex = 0;
		const Vector3& vPos = camInterface::GetPos();
		char garageName[GARAGE_NAME_MAX];
		snprintf(garageName, GARAGE_NAME_MAX, "New garage %d", newGarageIndex);
		garageName[GARAGE_NAME_MAX - 1] = '\0';
		newGarageIndex++;
		s32 idx = CGarages::AddOne(vPos.x, vPos.y, vPos.z - DEFAULT_GARAGE_HEIGHT / 2.f, DEFAULT_GARAGE_WIDTH, 0.f, 0.f, DEFAULT_GARAGE_DEPTH, vPos.z + DEFAULT_GARAGE_HEIGHT / 2.f, GARAGE_AMBIENT_PARKING, garageName, atHashString((u32)0), false, false, -1, -1);
		--idx;
		ms_pBoxListWindow->SetNumEntries(CGarages::aGarages.GetCount());
		ms_pBoxListWindow->SetCurrentIndex(idx);
		RefreshTweakVals();
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	DeleteCurrentGarageCB
// PURPOSE:		deletes the currently selected garage
//////////////////////////////////////////////////////////////////////////

void CGarageEditor::DeleteCurrentGarageCB()
{
	if (ms_pBoxListWindow && ms_pBoxListWindow->GetCurrentIndex() != -1)
	{
		int garageIndex = ms_pBoxListWindow->GetCurrentIndex();
		if (garageIndex < CGarages::aGarages.GetCount())
		{
			CGarages::aGarages.Delete(garageIndex);
			--garageIndex;
			garageIndex = MAX(0, garageIndex);
			ms_pBoxListWindow->SetNumEntries(CGarages::aGarages.GetCount());
			ms_pBoxListWindow->SetCurrentIndex(garageIndex);
			RefreshTweakVals();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	SetNameCB
// PURPOSE:		sets a new name for the current selected garage
//////////////////////////////////////////////////////////////////////////

void CGarageEditor::SetNameCB()
{
	if (ms_pBoxListWindow && ms_pBoxListWindow->GetCurrentIndex() != -1 && ms_achCurrentGarageName[0] != '\0')
	{
		CGarages::aGarages[ms_pBoxListWindow->GetCurrentIndex()].name.SetFromString(ms_achCurrentGarageName);
	}
}



//////////////////////////////////////////////////////////////////////////
// FUNCTION:	SetOwnerCB
// PURPOSE:		sets a new owner for the current selected garage
//////////////////////////////////////////////////////////////////////////

void CGarageEditor::SetOwnerCB()
{
	if (ms_pBoxListWindow && ms_pBoxListWindow->GetCurrentIndex() != -1 && ms_achCurrentGarageName[0] != '\0')
	{
		u32 ownerNameHash = GetOwnerHashFromIDX(ms_OwnerSelection);
		CGarages::aGarages[ms_pBoxListWindow->GetCurrentIndex()].owner.SetHash(ownerNameHash);
	}
}

// Note the IDX is local to the editor
u32 CGarageEditor::GetOwnerIDXFromHash(u32 hash)
{
	if(hash != 0)
	{
		u32 playerhash = MI_PLAYERPED_PLAYER_ZERO.GetName().GetHash();
		if(hash == playerhash)
		{
			return 1;	// Michael
		}

		playerhash = MI_PLAYERPED_PLAYER_ONE.GetName().GetHash();
		if(hash == playerhash)
		{
			return 2;	// Franklin
		}

		playerhash = MI_PLAYERPED_PLAYER_TWO.GetName().GetHash();
		if(hash == playerhash)
		{
			return 3;	// Trevor
		}
	}
	return 0;	// Any	
}

u32 CGarageEditor::GetOwnerHashFromIDX(u32 idx)
{
	u32	hash = 0;	// Any
	switch(idx)
	{
	case 1:
		{
			// Michael
			hash = MI_PLAYERPED_PLAYER_ZERO.GetName().GetHash();
			break;
		}
	case 2:
		{
			// Franklin
			hash = MI_PLAYERPED_PLAYER_ONE.GetName().GetHash();
			break;
		}
	case 3:
		{
			// Trevor
			hash = MI_PLAYERPED_PLAYER_TWO.GetName().GetHash();
			break;
		}
	default:
		{
			break;
		}
	}
	return hash;
}




//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
// FUNCTION:	MPGarageFlagSetCB
// PURPOSE:		
//////////////////////////////////////////////////////////////////////////

void CGarageEditor::MPGarageFlagSetCB()
{
	if (ms_pBoxListWindow && ms_pBoxListWindow->GetCurrentIndex() != -1 && ms_achCurrentGarageName[0] != '\0')
	{
		CGarage& g = CGarages::aGarages[ms_pBoxListWindow->GetCurrentIndex()];

		g.m_IsMPGarage = ms_bMPGarageFlag;

		if( !ms_bMPGarageFlag )
		{
			ExteriorFlagSetCB();
			InteriorFlagSetCB();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	EnclosedGarageFlagSetCB
// PURPOSE:		
//////////////////////////////////////////////////////////////////////////

void CGarageEditor::EnclosedGarageFlagSetCB()
{
	if (ms_pBoxListWindow && ms_pBoxListWindow->GetCurrentIndex() != -1 && ms_achCurrentGarageName[0] != '\0')
	{
		CGarage& g = CGarages::aGarages[ms_pBoxListWindow->GetCurrentIndex()];

		g.m_IsEnclosed = ms_bEnclosedGarageFlag;
	}
}


//////////////////////////////////////////////////////////////////////////
// FUNCTION:	ExteriorFlagSetCB/InteriorFlagSetCB
// PURPOSE:		
//////////////////////////////////////////////////////////////////////////

void CGarageEditor::ExteriorFlagSetCB()
{
	if (ms_pBoxListWindow && ms_pBoxListWindow->GetCurrentIndex() != -1 && ms_achCurrentGarageName[0] != '\0')
	{
		CGarage& g = CGarages::aGarages[ms_pBoxListWindow->GetCurrentIndex()];

		if( ms_bMPGarageFlag )
		{
			if( ms_bExteriorFlag )
			{
				if( g.m_InteriorBoxIDX == ms_selectedBox ) // Is this box also the interior box?
				{
					// Turn off interior flag on this box
					ms_bInteriorFlag = false;		// Does setting this call the callback? (NO)
					InteriorFlagSetCB();
				}

				g.m_ExteriorBoxIDX = (s8)ms_selectedBox;
			}
			else
			{
				g.m_ExteriorBoxIDX = -1;	// No index
			}
		}
		else
		{
			ms_bExteriorFlag = false;	// Balls, this change isn't reflected in the UI. Look into greying out?
			g.m_ExteriorBoxIDX = -1;	// No index
		}

	}
}

void CGarageEditor::InteriorFlagSetCB()
{
	if (ms_pBoxListWindow && ms_pBoxListWindow->GetCurrentIndex() != -1 && ms_achCurrentGarageName[0] != '\0')
	{
		CGarage& g = CGarages::aGarages[ms_pBoxListWindow->GetCurrentIndex()];

		if( ms_bMPGarageFlag )
		{
			if( ms_bInteriorFlag )
			{
				if( g.m_ExteriorBoxIDX == ms_selectedBox ) // Is this box also the exterior box?
				{
					// Turn off interior flag on this box
					ms_bExteriorFlag = false;
					ExteriorFlagSetCB();
				}
				g.m_InteriorBoxIDX = (s8)ms_selectedBox;
			}
			else
			{
				g.m_InteriorBoxIDX = -1;	// No index
			}
		}
		else
		{
			ms_bInteriorFlag = false;	// Balls, this change isn't reflected in the UI. Look into greying out?
			g.m_InteriorBoxIDX = -1;	// No index
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	RefreshTweakVals
// PURPOSE:		update the tweak rag controls to current selected box
//////////////////////////////////////////////////////////////////////////
void CGarageEditor::RefreshTweakVals() 
{
	if (ms_pBoxListWindow && 
		ms_pBoxListWindow->GetCurrentIndex() != -1 && 
		ms_pBoxListWindow->GetCurrentIndex() < CGarages::aGarages.GetCount())
	{
		CGarage& g = CGarages::aGarages[ms_pBoxListWindow->GetCurrentIndex()];
		sprintf(ms_achCurrentGarageName, "%s", g.name.GetCStr());

		if (ms_selectedBox >= g.m_boxes.GetCount())
		{
			ms_selectedBox = g.m_boxes.GetCount() - 1;
		}
		else if (ms_selectedBox < 0 )
		{
			ms_selectedBox = 0;
		}

		CGarage::Box &box = g.m_boxes[ms_selectedBox];
		ms_baseX = box.BaseX;
		ms_baseY = box.BaseY;
		ms_baseZ = box.BaseZ;
		ms_delta1X = box.Delta1X;
		ms_delta1Y = box.Delta1Y;
		ms_delta2X = box.Delta2X;
		ms_delta2Y = box.Delta2Y;
		ms_ceilingZ = box.CeilingZ;
		ms_type = (s32)g.type;
		ms_useLineIntersection = box.useLineIntersection;

		if (g.m_permittedVehicleType == VEHICLE_TYPE_HELI)
		{
			ms_permittedVehicleType = 1;
		}
		else if (g.m_permittedVehicleType == VEHICLE_TYPE_BOAT)
		{
			ms_permittedVehicleType = 2;
		}
		else
		{
			ms_permittedVehicleType = 0;
		}

		// MP Garage flag
		ms_bMPGarageFlag = g.m_IsMPGarage;

		// Interior exterior boxes ID's
		ms_bExteriorFlag = ms_bInteriorFlag = false;
		if(g.m_ExteriorBoxIDX == ms_selectedBox)
		{
			ms_bExteriorFlag = true;
		}
		if(g.m_InteriorBoxIDX == ms_selectedBox)
		{
			ms_bInteriorFlag = true;
		}

		ms_startWithVehicleSavingEnable = g.Flags.bSavingVehiclesWasEnabledBeforeSave;
		ms_vehicleSavingEnable = g.Flags.bSavingVehiclesEnabled;
		ms_OwnerSelection = GetOwnerIDXFromHash(g.owner.GetHash());
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Save
// PURPOSE:		saves the meta file containing garage data, and updates ui
//////////////////////////////////////////////////////////////////////////

void CGarageEditor::Save()
{
	CGarages::Save();

	if (ms_pBoxListWindow)
	{
		ms_pBoxListWindow->SetNumEntries(CGarages::aGarages.GetCount());

		if (CGarages::aGarages.GetCount() > 0)
		{
			RefreshTweakVals();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Load
// PURPOSE:		loads the meta file containing garage data, and updates ui
//////////////////////////////////////////////////////////////////////////

void CGarageEditor::Load()
{
	CGarages::aGarages.Reset();
	CGarages::NumSafeHousesRegistered = 0;

	ms_selectedBox = 0;	

	CGarages::Load();

	if (ms_pBoxListWindow)
	{
		ms_pBoxListWindow->SetNumEntries(CGarages::aGarages.GetCount());

		if (CGarages::aGarages.GetCount() > 0)
		{
			ResetCurrentGarageBBCallback();
			RefreshTweakVals();
		}
	}
}

#endif	//__BANK
