// 
// animation/debug/AnimViewer.cpp
// 
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved. 
// 

#if __BANK

#include "animation/debug/AnimViewer.h"

// C includes
#include <functional>

// rage includes
#include "atl/slist.h"
#include "bank/bank.h"
#include "bank/bkmgr.h"
#include "bank/bkseparator.h"
#include "bank/bkvector.h"
#include "bank/combo.h"
#include "bank/msgbox.h"
#include "bank/slider.h"
#include "bank/text.h"
#include "bank/title.h"
#include "cranimation/animtrack.h"
#include "cranimation/frameiterators.h"
#include "cranimation/vcrwidget.h"
#include "cranimation/weightset.h"
#include "crclip/clip.h"
#include "crclip/clipanimation.h"
#include "crclip/clipanimations.h"
#include "creature/componentblendshapes.h"
#include "creature/componentextradofs.h"
#include "creature/componentphysical.h"
#include "creature/componentshadervars.h" 
#include "creature/creature.h"
#include "crextra/expression.h"
#include "crmetadata/properties.h"
#include "crmetadata/property.h"
#include "crmetadata/propertyattributes.h"
#include "crmetadata/tagiterators.h"
#include "crmetadata/tags.h"
#include "crmetadata/dumpoutput.h"
#include "crmotiontree/nodeanimation.h"
#include "crmotiontree/nodeblendn.h "
#include "crmotiontree/nodeblend.h"
#include "crmotiontree/nodeclip.h"
#include "crmotiontree/nodefilter.h"
#include "crmotiontree/nodeexpression.h"
#include "crmotiontree/nodemerge.h"
#include "crmotiontree/nodepm.h"
#include "crparameterizedmotion/blenddatabase.h"
#include "crparameterizedmotion/parameterizedmotiondictionary.h"
#include "crparameterizedmotion/parameterizedmotionsimple.h"
#include "crparameterizedmotion/parametersampler.h"
#include "crparameterizedmotion/pointcloud.h"
#include "crskeleton/skeleton.h"
#include "crskeleton/skeletondata.h"
#include "file/default_paths.h"
#include "fwanimation/directorcomponentcreature.h"
#include "fwanimation/directorcomponentmove.h"
#include "fwanimation/directorcomponentragdoll.h"
#include "fwanimation/directorcomponentsyncedscene.h"
#include "fwanimation/filter.h"
#include "fwanimation/expressionsets.h"
#include "fwanimation/facialclipsetgroups.h"
#include "fwdebug/picker.h"
#include "grcore/debugdraw.h"
#include "grcore/font.h"
#include "fwscene/stores/blendshapestore.h"
#include "fwscene/stores/expressionsdictionarystore.h"
#include "fwscene/stores/framefilterdictionarystore.h"
#include "grblendshapes/blendshapes.h"
#include "move/move_clip.h"
#include "move/move_synchronizer.h"
#include "move/move_state.h"
#include "parser/manager.h"
#include "phbound/boundcomposite.h"
#include "fwanimation/posematcher.h"
#include "fwanimation/clipsets.h"

#if CALLSTACK_ON_BLOCKING_ANIM_PREPHYSICS_UPDATE
#include "fwanimation/directorcomponentmotiontree.h"
#endif //CALLSTACK_ON_BLOCKING_ANIM_PREPHYSICS_UPDATE

// project includes
#include "animation/anim_channel.h"
#include "animation/AnimBones.h"
#include "animation/AnimDefines.h"
#include "fwanimation/animdirector.h"
#include "fwanimation/animmanager.h"
#include "animation/debug/AnimDebug.h"
#include "animation/debug/AnimPlacementTool.h"
#include "animation/debug/ClipEditor.h"
#include "animation/debug/GestureEditor.h"
#include "animation/FacialData.h"
#include "animation/MoveObject.h"
#include "animation/MoveVehicle.h"
#include "animation/MovePed.h"
#include "fwanimation/pointcloud.h"
#include "fwanimation/directorcomponentexpressions.h"
#include "fwanimation/directorcomponentmotiontree.h"
#include "animation/PMDictionaryStore.h"
#include "audio/speechaudioentity.h"
#include "camera/CamInterface.h"
#include "camera/debug/DebugDirector.h"
#include "camera/debug/FreeCamera.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/gameplay/aim/FirstPersonShooterCamera.h"
#include "camera/scripted/BaseSplineCamera.h"
#include "camera/system/CameraMetadata.h"
#include "camera/viewports/ViewportManager.h"
#include "cutscene/CutSceneManagerNew.h"
#include "cutscene/AnimatedModelEntity.h"
#include "Debug/DebugGlobals.h"
#include "Debug/debugscene.h"
#include "grblendshapes/manager.h"
#include "grblendshapes/target.h"
#include "ik/IkManager.h"
#include "ModelInfo/modelInfo.h"
#include "ModelInfo/ModelInfo_Factories.h"
#include "peds/PedFactory.h"
#include "peds/PedIntelligence.h"
#include "peds/PedMoveBlend/PedMoveBlendManager.h"
#include "peds/PedMoveBlend/PedMoveBlendOnFoot.h"
#include "peds/rendering/pedVariationPack.h"
#include "peds/rendering/PedVariationDebug.h"
#include "peds/rendering/PedVariationStream.h"
#include "peds/Ped.h"
#include "Peds/PedDebugVisualiser.h"
#include "physics/physics.h"
#include "pharticulated/articulatedbody.h"
#include "renderer/Entities/PedDrawHandler.h" // for CPedDrawHandler::SetEnableRendering
#include "Renderer/renderer.h"
#include "renderer/RenderThread.h"
#include "renderer/RenderPhases/RenderPhaseDebugOverlay.h"
#include "scene/AnimatedBuilding.h"
#include "scene/world/GameWorld.h"
#include "scene/world/VisibilityMasks.h"
#include "script/commands_object.h"
#include "shaders/CustomShaderEffectPed.h"
#include "streaming/streaming.h"				// For CStreaming::LoadAllRequestedObjects(), maybe other things.
#include "task/Animation/TaskScriptedAnimation.h"
#include "task/Combat/TaskDamageDeath.h"
#include "task/motion/Locomotion/taskmotionped.h"
#include "task/Movement/TaskGetUp.h"
#include "system/bootmgr.h"
#include "system/namedpipe.h"
#include "system/pipepacket.h"
#include "Task/System/Task.h"
#include "Task/System/TaskTypes.h"
#include "Task/Default/AmbientAnimationManager.h"
#include "Task/Default/TaskAmbient.h"
#include "Task/General/TaskBasic.h"
#include "Task/Default/TaskPlayer.h"
#include "Task/Movement/TaskGetUp.h"
#include "task/physics/blendfromnmdata.h"
#include "Task/Physics/TaskBlendFromNM.h"
#include "text/TextConversion.h"
#include "text/Text.h"
#include "vehicleAi/VehicleIntelligence.h"
#include "vehicles/vehicle.h"

ANIM_OPTIMISATIONS()

#define NUM_DEBUG_MATRICES	500

namespace rage
{
#if __PFDRAW
	EXT_PFD_DECLARE_GROUP(Physics);
	EXT_PFD_DECLARE_ITEM(Active);
	EXT_PFD_DECLARE_ITEM(Inactive);
	EXT_PFD_DECLARE_ITEM(Fixed);
#endif

#if DEBUG_EXPRESSIONS
XPARAM(debugexpr);
#endif // DEBUG_EXPRESSIONS
}

u32 GetMotionTreeCaptureCount()
{
	u32 captureMotionTreeCount = 0;
	PARAM_capturemotiontreecount.Get(captureMotionTreeCount);
	captureMotionTreeCount = rage::Max(captureMotionTreeCount, (u32)1);
	return captureMotionTreeCount;
}

//////////////////////////////////////////////////////////////////////////
//	Anim viewer static declarations
//////////////////////////////////////////////////////////////////////////
// Anim debug
fwDebugBank* CAnimViewer::m_pBank = NULL;
//bkBank* CAnimViewer::m_pPreviewBank = NULL;
#if ENABLE_BLENDSHAPES
bkBank* CAnimViewer::m_pBlendshapeBank = NULL;
#endif // ENABLE_BLENDSHAPES

//bkCombo* CAnimViewer::m_pAnimCombo = NULL;
bkCombo* CAnimViewer::m_pBoneCombo[NUM_BONE_COMBOS];
bkCombo* CAnimViewer::m_pSpringBoneCombo = NULL;
bkVector3* CAnimViewer::m_pSpringGravity = NULL;
bkVector3* CAnimViewer::m_pSpringDirection = NULL;
#if ENABLE_BLENDSHAPES
bkCombo* CAnimViewer::m_pBlendshapeCombo = NULL;
#endif // ENABLE_BLENDSHAPES
int CAnimViewer::ms_iSelectedMemoryGroup = 0;
bool CAnimViewer::m_filterByMemoryGroup = false;

bkGroup* CAnimViewer::m_pLinearPhysicalExpressionGroup = NULL;

CAnimViewer::SpringDof CAnimViewer::m_linearSpringDofs;
CAnimViewer::SpringDof CAnimViewer::m_angularSpringDofs;

RegdDyn CAnimViewer::m_pDynamicEntity(NULL);

Vector3 CAnimViewer::m_componentRoot(1.0f, 1.0f, 1.0f);

Vector3 CAnimViewer::m_rotation(0.0f, 0.0f, 0.0f);
Vector3 CAnimViewer::m_translation(0.0f, 0.0f, 0.0f);
Vector3 CAnimViewer::m_scale(1.0f, 1.0f, 1.0f);

int CAnimViewer::m_modelIndex = 0;
int CAnimViewer::m_motionClipSetIndex = 0;
int CAnimViewer::m_weaponClipSetIndex = 0;
int CAnimViewer::m_facialClipSetIndex = 0;
fwClipSetRequestHelper CAnimViewer::m_facialClipSetHelper;
bool CAnimViewer::m_bEnableFacial = true;

//int CAnimViewer::m_animationIndex = 0;
//int CAnimViewer::m_animationDictionaryIndex = 0;

bool CAnimViewer::m_bNeedsParamWidgetOptionsRebuild = false;

int CAnimViewer::m_pmIndex = 0;
int CAnimViewer::m_pmDictionaryIndex = 0;
float CAnimViewer::m_OldPhase = 0.0f;
float CAnimViewer::m_Phase = 0.0f;
float CAnimViewer::m_Frame = 0.0f;
float CAnimViewer::m_Rate = 1.0f;
float CAnimViewer::m_AnimBlendPhase = 0.0f;
float CAnimViewer::m_OldAnimBlendPhase = 0.0f;
float CAnimViewer::m_BlendPhase = 0.0f;

atHashString CAnimViewer::m_AnimHash; 
atHashString CAnimViewer::m_BlendAnimHash;
bkSlider* CAnimViewer::m_pFrameSlider = NULL; 

atVector<CAnimViewer::sPmDictionary> CAnimViewer::m_pmDictionary;
atArray<const char*> CAnimViewer::m_pmDictionaryNames;

atArray<const char*> CAnimViewer::m_boneNames;
atArray<const char*> CAnimViewer::m_springNames;
#if ENABLE_BLENDSHAPES
atArray<const char*> CAnimViewer::m_blendShapeNames;
atArray<float> CAnimViewer::m_blendShapeBlends;
#endif // ENABLE_BLENDSHAPES
atVector<atString> CAnimViewer::m_modelStrings;
atArray<const char*> CAnimViewer::m_modelNames;

atArray<CAnimViewer::sBoneHistory> CAnimViewer::m_boneHistoryArray;

int CAnimViewer::m_matIndex = 0;

bool CAnimViewer::m_unInterruptableTask = false;
bool CAnimViewer::m_bLockPosition = false;
bool CAnimViewer::m_bLockRotation = false;
bool CAnimViewer::m_bResetMatrix = false;
bool CAnimViewer::m_extractZ = false;
bool CAnimViewer::m_advancedMode = true;

EXTERN_PARSER_ENUM(eCurveType);
extern const char* parser_eCurveType_Strings[];

EXTERN_PARSER_ENUM(eIkControlFlags);
extern const char* parser_eIkControlFlags_Strings[];

EXTERN_PARSER_ENUM(eScriptedAnimFlags);
extern const char* parser_eScriptedAnimFlags_Strings[];

eScriptedAnimFlagsBitSet CAnimViewer::ms_AnimFlags;
eIkControlFlagsBitSet CAnimViewer::ms_IkControlFlags;

CDebugFilterSelector CAnimViewer::ms_taskFilterSelector;

float CAnimViewer::ms_fPreviewAnimTaskBlendIn = 0.25f;
float CAnimViewer::ms_fPreviewAnimTaskBlendOut = 0.25f;
float CAnimViewer::ms_fPreviewAnimTaskStartPhase = 0.0f;

bool CAnimViewer::ms_bPreviewAnimTaskAdvanced = false;
Vector3 CAnimViewer::ms_vPreviewAnimTaskAdvancedPosition = VEC3_ZERO;
Vector3 CAnimViewer::ms_vPreviewAnimTaskAdvancedOrientation = VEC3_ZERO;

bool CAnimViewer::ms_bPreviewNetworkAdditive = false;
bool CAnimViewer::ms_bPreviewNetworkDisableIkSolvers = true;

bool CAnimViewer::m_renderSkeleton = false;
bool CAnimViewer::m_renderSkin = true;
bool CAnimViewer::m_active = false;
bool CAnimViewer::ms_bRenderEntityInfo = false;

#if FPS_MODE_SUPPORTED
bool CAnimViewer::m_renderThirdPersonSkeleton = false;
#endif // FPS_MODE_SUPPORTED

s32 CAnimViewer::m_entityScriptIndex = -1;
s32 CAnimViewer::m_boneIndex = 0;
s32 CAnimViewer::m_springIndex = 0;
bool CAnimViewer::ms_bApplyOverrideRelatively = true;
bool CAnimViewer::ms_bOverrideBoneRotation = false;
bool CAnimViewer::ms_bOverrideBoneTranslation = false;
bool CAnimViewer::ms_bOverrideBoneScale = false;
float CAnimViewer::ms_fOverrideBoneBlend = 1.0f;

bool CAnimViewer::m_drawSelectedEntityMat = false;
float CAnimViewer::m_drawSelectedEntityMatSize = 0.5f;
bool CAnimViewer::m_drawSelectedEntityMatTranslation = false;
bool CAnimViewer::m_drawSelectedEntityMatRotation = false;
bool CAnimViewer::m_drawSelectedEntityMatTransAndRotWrtCutscene = false;

bool CAnimViewer::m_drawSelectedEntityMoverTranslation = false;
bool CAnimViewer::m_drawSelectedEntityMoverRotation = false;
bool CAnimViewer::m_drawSelectedEntityHair = false;
bool CAnimViewer::m_drawSelectedEntityHairMeasure = false;
float CAnimViewer::m_drawSelectedEntityHairMeasureHeight = 0.0f;

bool CAnimViewer::m_drawSelectedBoneMat = false;
bool CAnimViewer::m_drawSelectedParentBoneMat = false;
bool CAnimViewer::m_drawVehicleComponentBoundingBox = false;
bool CAnimViewer::m_drawSelectedBoneMatTranslation = false;
bool CAnimViewer::m_drawSelectedBoneMatRotation = false;
bool CAnimViewer::m_drawSelectedBoneMatTransAndRotWrtCutscene = false;
bool CAnimViewer::m_drawSelectedBoneMatScale = false;
float CAnimViewer::m_drawSelectedBoneMatSize = 0.5f;
bool CAnimViewer::m_drawSelectedBoneJointLimits = false;
bool CAnimViewer::m_drawFirstPersonCamera = false;
bool CAnimViewer::m_drawConstraintHelpers = false;
bool CAnimViewer::m_drawMotionBlurData = false;
bool CAnimViewer::m_drawAllBonesMat = false;
float CAnimViewer::m_drawAllBonesMatSize = 0.01f;

//mover track rendering data
bool CAnimViewer::m_drawMoverTrack = false;
bool CAnimViewer::m_renderMoverTrackAtEntity = true;
float CAnimViewer::m_drawMoverTrackSize = 0.175f;
Matrix34 CAnimViewer::m_moverTrackTransform;

bool CAnimViewer::m_renderTagsForSelectedClip = false;
bool CAnimViewer::m_renderTagsVisuallyForSelectedClip = false;
float CAnimViewer::m_renderTagsVisuallyForSelectedClipOffset = 0.5f;

bool CAnimViewer::m_degrees = true;
bool CAnimViewer::m_eulers = true;

bool CAnimViewer::m_drawDebugSkeletonArray = false;
bool CAnimViewer::m_drawDebugSkeletonsUsingLocals = false;
bool CAnimViewer::m_drawSelectedAnimSkeleton = false;
float CAnimViewer::m_drawSelectedAnimSkeletonPhase = 0.0f;
float CAnimViewer::m_drawSelectedAnimSkeletonOffset = 0.0f;

bool CAnimViewer::m_drawSelectedAnimSkeletonDOFTest = false;
u8	 CAnimViewer::m_drawSelectedAnimSkeletonDOFTestTrack = kTrackMoverTranslation;
u16	 CAnimViewer::m_drawSelectedAnimSkeletonDOFTestId = 0;
u8	 CAnimViewer::m_drawSelectedAnimSkeletonDOFTestType = kFormatTypeVector3;

bool CAnimViewer::m_renderBoneHistory = true;
bool CAnimViewer::m_autoAddBoneHistoryEveryFrame = false;

#ifdef RS_BRANCHSUFFIX
char CAnimViewer::m_BatchProcessInputFolder[RAGE_MAX_PATH] = "art:/ng/anim/export_mb";
char CAnimViewer::m_BatchProcessOutputFolder[RAGE_MAX_PATH] = "art:/ng/anim/update_mb";
#else
char CAnimViewer::m_BatchProcessInputFolder[RAGE_MAX_PATH] = "art:/anim/export_mb";
char CAnimViewer::m_BatchProcessOutputFolder[RAGE_MAX_PATH] = "art:/anim/update_mb";
#endif
bool CAnimViewer::m_abClipProcessEnabled[CP_MAX] = { 0 };
bool CAnimViewer::m_bDefaultHighHeelControlToOne = true;
bool CAnimViewer::m_bVerbose = true;
CAnimViewer::FindAndReplaceData CAnimViewer::m_FindAndReplaceData = { 0 };
CAnimViewer::ConstraintHelperValidation CAnimViewer::m_ConstraintHelperValidation = { 0.08f, true };
#if __DEV

//point cloud rendering settings
bool CAnimViewer::m_renderPointCloud = false;
float CAnimViewer::m_pointCloudBoneSize = 0.05f;
bool CAnimViewer::m_renderPointCloudAtSkelParent = true;
bool CAnimViewer::m_renderLastMatchedPose = false;

#endif //__DEV

bool CAnimViewer::m_renderSynchronizedSceneDebug = false;
int CAnimViewer::m_renderSynchronizedSceneDebugHorizontalScroll = 0;
int CAnimViewer::m_renderSynchronizedSceneDebugVerticalScroll = 0;

bool CAnimViewer::m_renderFacialVariationDebug = false;
int CAnimViewer::m_renderFacialVariationTrackerCount = 4;

bool CAnimViewer::m_renderFrameFilterDictionaryDebug = false;

s32 CAnimViewer::ms_iSeat = 0;
bool CAnimViewer::ms_bComputeOffsetFromAnim = false;
float CAnimViewer::ms_fExtraZOffsetForBothPeds = 0.0f;
float CAnimViewer::ms_fXRelativeOffset = -0.72f;
float CAnimViewer::ms_fYRelativeOffset = -0.37f;
float CAnimViewer::ms_fZRelativeOffset = 0.45f;
float CAnimViewer::ms_fRelativeHeading = 0.0f;

atArray<CDebugClipDictionary*>	CAnimViewer::ms_aFirstPedDicts;
atArray<CDebugClipDictionary*>	CAnimViewer::ms_aSecondPedDicts;
atArray<const char*>			CAnimViewer::ms_szFirstPedsAnimNames;
atArray<const char*>			CAnimViewer::ms_szSecondPedsAnimNames;

Vector2 CAnimViewer::m_vScale(0.25f, 0.3f);
int CAnimViewer::m_iNoOfTexts = 0;
float CAnimViewer::m_fVertDist = 0.02f;
float CAnimViewer::m_fHorizontalBorder = 0.04f;
float CAnimViewer::m_fVerticalBorder = 0.04f;

#if ENABLE_BLENDSHAPES
int CAnimViewer::m_blendShapeIndex = 0;
float CAnimViewer::m_blendShapeBlend = 0.0f;
bool CAnimViewer::m_overrideBlendShape = false;
bool CAnimViewer::m_displayBlendShape = false;
#endif // ENABLE_BLENDSHAPES

atArray<CAnimViewer::sDebugSkeleton> CAnimViewer::m_debugSkeletons;

int CAnimViewer::m_skeletonIndex = 0;
bool CAnimViewer::m_autoAddSkeletonEveryFrame = false;
bool CAnimViewer::m_renderDebugSkeletonNames = true;
bool CAnimViewer::m_renderDebugSkeletonParentMatrices = true;

atHashString CAnimViewer::ms_hashString;
char CAnimViewer::ms_hashValue[16];

audGestureData CAnimViewer::ms_gestureData;
atHashString CAnimViewer::ms_gestureClipNameHash;
int CAnimViewer::m_gestureClipSetIndex = 0;

bool CAnimViewer::ms_bRenderCameraCurve = false;
float CAnimViewer::ms_fRenderCameraCurveP1X = 0.0f;
float CAnimViewer::ms_fRenderCameraCurveP1Y = 0.0f;
float CAnimViewer::ms_fRenderCameraCurveP2X = 1.0f;
float CAnimViewer::ms_fRenderCameraCurveP2Y = 1.0f;
float CAnimViewer::ms_fRenderCameraCurveScale = 0.8f;
float CAnimViewer::ms_fRenderCameraCurveOffsetX = 0.1f;
float CAnimViewer::ms_fRenderCameraCurveOffsetY = 0.1f;
int CAnimViewer::ms_iSelectedCurveType = 0;
bool CAnimViewer::ms_bShouldBlendIn = true;

s32 CAnimViewer::ms_DofTrackerBoneIndices[MAX_DOFS_TO_TRACK];
bool CAnimViewer::ms_DofTrackerUpdatePostPhysics = false;

Vector3 CAnimViewer::m_startPosition = Vector3(0.0f, 0.0f, 0.0f);

// motion tree visualization
bool CAnimViewer::m_bVisualiseMotionTree = false;
fwAnimDirectorComponent::ePhase CAnimViewer::m_eVisualiseMotionTreePhase = fwAnimDirectorComponent::kPhasePrePhysics;
bool CAnimViewer::m_bVisualiseMotionTreeNewStyle = true;
bool CAnimViewer::m_bVisualiseMotionTreeSimple = false;
bool CAnimViewer::m_bDumpMotionTreeSimple = false;
bool CAnimViewer::m_bCaptureMotionTree = false;
bool CAnimViewer::m_bCapturingMotionTree = false;
bool CAnimViewer::m_bCaptureDofs = false;
int CAnimViewer::m_iCaptureFilterNameIndex = 0;
int CAnimViewer::m_iCaptureFilterNameCount = 0;
const char **CAnimViewer::m_szCaptureFilterNames = NULL;
bool CAnimViewer::m_bRelativeToParent = false;
bool CAnimViewer::m_bApplyCapturedDofs = false;
u32 CAnimViewer::m_uNextApplyCaptureDofsPreRenderFrame = 0;
bool CAnimViewer::m_bTrackLargestMotionTree = false;
u32 CAnimViewer::m_uRenderMotionTreeCaptureFrame = 0;
char CAnimViewer::m_szMotionTreeCapture[256];
u32 CAnimViewer::m_uMotionTreeCaptureLength = 256;
fwMoveDumpFrameSequence *CAnimViewer::m_MoveDump = NULL;

// entity capture
bkGroup *CAnimViewer::m_pEntitiesToCaptureGroup = NULL;
atVector< CEntityCapture * > CAnimViewer::m_EntityCaptureArray;
bool CAnimViewer::m_EntityCaptureLock30Fps = false;
bool CAnimViewer::m_EntityCaptureLock60Fps = false;

// camera capture
int CAnimViewer::m_iCameraCapture_CameraIndex = 0;
char CAnimViewer::m_szCameraCapture_AnimationName[256] = "Camera";
u32 CAnimViewer::m_uCameraCapture_AnimationNameLength = 256;
atArray< const crFrame * > CAnimViewer::m_CameraCapture_FrameArray;
Matrix34 CAnimViewer::m_CameraCapture_InitialMover;
int CAnimViewer::m_iCameraCapture_CameraNameIndex = 0;
int CAnimViewer::m_iCameraCapture_CameraNameCount = 0;
const char **CAnimViewer::m_szCameraCapture_CameraNames = NULL;
bool CAnimViewer::m_bCameraCapture_Started = false;

// blend-out capture
atSList<CAnimViewer::BlendOutData> CAnimViewer::m_BlendOutData;
bkGroup* CAnimViewer::m_BlendOutDataGroup = NULL;
bool CAnimViewer::m_BlendOutDataEnabled = false;
char CAnimViewer::m_BlendOutAnimationName[RAGE_MAX_PATH] = "RagdollPose";
Vector3 CAnimViewer::m_BlendOutOrientationOffset(Vector3::ZeroType);
Vector3 CAnimViewer::m_BlendOutTranslationOffset(Vector3::ZeroType);
bool CAnimViewer::m_bForceBlendoutSet;
eNmBlendOutSet CAnimViewer::m_ForcedBlendOutSet;

PARAM(blendoutdataenabled, "Enable blend out data capturing");

//expression debugging
CAnimViewer::sDebugExpressions CAnimViewer::m_debugMidPhysicsExpressions(fwAnimDirectorComponent::kPhaseMidPhysics);
CAnimViewer::sDebugExpressions CAnimViewer::m_debugPreRenderExpressions(fwAnimDirectorComponent::kPhasePreRender);
bkGroup* CAnimViewer::m_pExpressionWidgetGroup = NULL;
bkGroup* CAnimViewer::m_pExpressionParentGroup = NULL;

// debug rendering
int CAnimViewer::m_horizontalScroll = 5;
int CAnimViewer::m_verticalScroll = 9;
int CAnimViewer::m_dictionaryScroll = 0;

//anim viewer anim selector control
CDebugClipSelector CAnimViewer::m_animSelector(true, true, true);
CDebugClipSelector CAnimViewer::m_blendAnimSelector(true, true, true);

CDebugClipSelector CAnimViewer::m_additiveBaseSelector(true, false, false);
float CAnimViewer::m_fAdditiveBaseRate = 1.0f;
float CAnimViewer::m_fAdditiveBasePhase = 0.0f;
bool CAnimViewer::m_bAdditiveBaseLooping = false;
CDebugClipSelector CAnimViewer::m_additiveOverlaySelector(true, false, false);
float CAnimViewer::m_fAdditiveOverlayRate = 1.0f;
float CAnimViewer::m_fAdditiveOverlayPhase = 0.0f;
bool CAnimViewer::m_bAdditiveOverlayLooping = true;

//preview camera
bool CAnimViewer::ms_bUsePreviewCamera = false;
bool CAnimViewer::ms_bUsingPreviewCamera = false;
int CAnimViewer::ms_iPreviewCameraIndex = 0;
int CAnimViewer::ms_iPreviewCameraCount = 3;
const char *CAnimViewer::ms_szPreviewCameraNames[] = { "Full", "Three Quarter", "Head" };

s32 fwMotionTreeTester::ms_maxComplexity = 0;
s32 fwMotionTreeTester::ms_autoPauseComplexity = 250;
s32 fwMotionTreeTester::ms_maxTreeDepth = 0;
s32 fwMotionTreeTester::ms_autoPauseTreeDepth = 50;

CDebugObjectSelector CAnimViewer::m_objectSelector;

CDebugFileSelector CAnimViewer::m_moveFileSelector(RS_BUILDBRANCH "/preview/", ".mrf");

RegdEnt CAnimViewer::m_spawnedObject;

CMoveNetworkHelper CAnimViewer::m_MoveNetworkHelper;

//u32 CAnimViewer::m_boneWeightSignature = NUM_EBONEMASK;

const fwMvBooleanId CAnimViewer::ms_LoopedId("looped",0x204169A7);
const fwMvClipId CAnimViewer::ms_ClipId("clip",0xE71BD32A);
const fwMvFloatId CAnimViewer::ms_PhaseId("phase",0xA27F482B);
const fwMvFloatId CAnimViewer::ms_PhaseOutId("phase_out",0xC152E4FC);
const fwMvFloatId CAnimViewer::ms_RateId("rate",0x7E68C088);
const fwMvFloatId CAnimViewer::ms_RateOutId("rate_out",0x9305A09E);

const fwMvClipId CAnimViewer::ms_BlendClipId("blendClip",0x4A0CEA4A);
const fwMvFloatId CAnimViewer::ms_BlendAnimPhaseId("blendAnimPhase",0xAD52B975);
const fwMvFloatId CAnimViewer::ms_BlendPhaseId("blendPhase",0xD45BF0DF);

//////////////////////////////////////////////////////////////////////////
// MoVE Network preview stuff
//////////////////////////////////////////////////////////////////////////

bkGroup* CAnimViewer::m_previewNetworkGroup = NULL;
bkGroup* CAnimViewer::m_clipSetVariableGroup = NULL;
bkGroup* CAnimViewer::m_widgetOptionsGroup = NULL;	
int CAnimViewer::m_newParamType = 0;		
char CAnimViewer::m_newParamName[64] = "";				// Stores the param name for new parameter widgets
char CAnimViewer::m_stateMachine[64] = "";
char CAnimViewer::m_forceToState[64] = "";

//int CAnimViewer::m_moveNetworkPrecedence = 1;
int CAnimViewer::m_moveNetworkClipSet = 0;
bool CAnimViewer::m_paramWidgetUpdateEveryFrame = true;
bool CAnimViewer::m_paramWidgetOutputMode = false;
bool CAnimViewer::ms_bCreateSecondary = false;
float CAnimViewer::m_paramWidgetMinValue = 0.0f;
float CAnimViewer::m_paramWidgetMaxValue = 1.0f;
int CAnimViewer::m_paramWidgetAnalogueControlElement = 0;
const char * CAnimViewer::m_analogueControlElementNames[7] = { 
	"None",
	"Left stick X",
	"Left stick Y",
	"Left trigger",
	"Right stick X",
	"Right stick Y",
	"right trigger"
};

int CAnimViewer::m_paramWidgetControlElement = 0;
const char * CAnimViewer::m_controlElementNames[7] = { 
	"None",
	"Left shoulder",
	"Right shoulder",
	"Button A or Cross",
	"Button B or Circle",
	"Button Y or Triangle",
	"Button X or Square"
};

#if CRMT_NODE_2_DEF_MAP
bool CAnimViewer::m_bEnableNetworksDebug = false;
char CAnimViewer::m_networkDebugNetworkFilter[256];
bkText *CAnimViewer::m_pNetworkNodePropertyData = NULL;
const int CAnimViewer::m_networkDebugDataLength = 1024;
char CAnimViewer::m_networkDebugData[CAnimViewer::m_networkDebugDataLength];
#endif //CRMT_NODE_2_DEF_MAP

fwClipRpfBuildMetadata g_ClipRpfBuildMetadata;

//const char * CAnimViewer::m_precedenceNames[CMovePed::kNumPrecedence] = 
//{
//	"kPrecedenceLow",
//	"kPrecedenceMid",
//	"kPrecedenceHigh",
//	"kPrecedenceAdditive"
//};

bool fwMotionTreeVisualiser::ms_bRenderTree = true;
bool fwMotionTreeVisualiser::ms_bRenderClass = true;
bool fwMotionTreeVisualiser::ms_bRenderWeight = true;
bool fwMotionTreeVisualiser::ms_bRenderLocalWeight = false;
bool fwMotionTreeVisualiser::ms_bRenderStateActiveName = false;
bool fwMotionTreeVisualiser::ms_bRenderInvalidNodes = false;
bool fwMotionTreeVisualiser::ms_bRenderSynchronizerWeights = false;
bool fwMotionTreeVisualiser::ms_bRenderSynchronizerGraph = false;
bool fwMotionTreeVisualiser::ms_bRenderRequests = false;
bool fwMotionTreeVisualiser::ms_bRenderFlags = false;
bool fwMotionTreeVisualiser::ms_bRenderInputParams = false;
bool fwMotionTreeVisualiser::ms_bRenderOutputParams = false;
bool fwMotionTreeVisualiser::ms_bRenderDofs = false;
bool fwMotionTreeVisualiser::ms_bRenderAddress = false;

float g_motionTreeVisualisationXIndent = 0.035f;
float g_motionTreeVisualisationYIndent = 0.035f;
float g_motionTreeVisualisationXIncrement = 0.005f;
float g_motionTreeVisualisationYIncrement = 0.010f;

//////////////////////////////////////////////////////////////////////////
// Old CAnimDebug static declarations
//////////////////////////////////////////////////////////////////////////

#define ANIMDEBUG_STATS (1)
#define ANIMASSOC_STATS (1)

#if ANIMDEBUG_STATS

PF_PAGE(CAnimationDofTrackerPage, "GTA Anim Dof Tracker");

PF_GROUP(MoverDeltaStats);
PF_LINK(CAnimationDofTrackerPage, MoverDeltaStats);

PF_VALUE_FLOAT(Mover_TransX,	MoverDeltaStats);
PF_VALUE_FLOAT(Mover_TransY,	MoverDeltaStats);
PF_VALUE_FLOAT(Mover_TransZ,	MoverDeltaStats);
PF_VALUE_FLOAT(Mover_RotX,		MoverDeltaStats);
PF_VALUE_FLOAT(Mover_RotY,		MoverDeltaStats);
PF_VALUE_FLOAT(Mover_RotZ,		MoverDeltaStats);

PF_GROUP(Bone1Stats);
PF_LINK(CAnimationDofTrackerPage, Bone1Stats);

PF_VALUE_FLOAT(Bone1_TransX,	Bone1Stats);
PF_VALUE_FLOAT(Bone1_TransY,	Bone1Stats);
PF_VALUE_FLOAT(Bone1_TransZ,	Bone1Stats);
PF_VALUE_FLOAT(Bone1_RotX,	Bone1Stats);
PF_VALUE_FLOAT(Bone1_RotY,	Bone1Stats);
PF_VALUE_FLOAT(Bone1_RotZ,	Bone1Stats);

PF_GROUP(Bone2Stats);
PF_LINK(CAnimationDofTrackerPage, Bone2Stats);

PF_VALUE_FLOAT(Bone2_TransX,	Bone2Stats);
PF_VALUE_FLOAT(Bone2_TransY,	Bone2Stats);
PF_VALUE_FLOAT(Bone2_TransZ,	Bone2Stats);
PF_VALUE_FLOAT(Bone2_RotX,	Bone2Stats);
PF_VALUE_FLOAT(Bone2_RotY,	Bone2Stats);
PF_VALUE_FLOAT(Bone2_RotZ,	Bone2Stats);

PF_GROUP(Bone3Stats);
PF_LINK(CAnimationDofTrackerPage, Bone3Stats);

PF_VALUE_FLOAT(Bone3_TransX,	Bone3Stats);
PF_VALUE_FLOAT(Bone3_TransY,	Bone3Stats);
PF_VALUE_FLOAT(Bone3_TransZ,	Bone3Stats);
PF_VALUE_FLOAT(Bone3_RotX,	Bone3Stats);
PF_VALUE_FLOAT(Bone3_RotY,	Bone3Stats);
PF_VALUE_FLOAT(Bone3_RotZ,	Bone3Stats);

namespace CAnimViewerProfile 
{
	PF_PAGE(CAnimViewerPage, "GTA AnimDebug");


	PF_GROUP(CAnimViewerStats);
	PF_LINK(CAnimViewerPage, CAnimViewerStats);

	PF_COUNTER(CAnimViewer_PedsOnscreen,			CAnimViewerStats);
	PF_COUNTER(CAnimViewer_PedsOffscreen,		CAnimViewerStats);
	PF_COUNTER(CAnimViewer_combinedOnScreen,		CAnimViewerStats);
	PF_COUNTER(CAnimViewer_combinedOffscreen,	CAnimViewerStats);
};

using namespace CAnimViewerProfile;
#endif // ANIMDEBUG_STATS

#if ANIMASSOC_STATS
namespace CAnimAssocProfile 
{
	PF_PAGE(CClipDictionaryMemoryGroupsPage, "GTA Clipdictionary memory groups");
	PF_GROUP(CClipDictionaryMemoryGroupsStats);
	PF_LINK(CClipDictionaryMemoryGroupsPage, CClipDictionaryMemoryGroupsStats);
	PF_COUNTER(MG_None_Counter,	CClipDictionaryMemoryGroupsStats);
	PF_COUNTER(MG_AmbientMovement_Counter,CClipDictionaryMemoryGroupsStats);
	PF_COUNTER(MG_CivilianReactions_Counter,CClipDictionaryMemoryGroupsStats);
	PF_COUNTER(MG_Combat_Counter,CClipDictionaryMemoryGroupsStats);
	PF_COUNTER(MG_CoreMovement_Counter,CClipDictionaryMemoryGroupsStats);
	PF_COUNTER(MG_Cover_Counter,CClipDictionaryMemoryGroupsStats);
	PF_COUNTER(MG_Creature_Counter,CClipDictionaryMemoryGroupsStats);
	PF_COUNTER(MG_Facial_Counter,CClipDictionaryMemoryGroupsStats);
	PF_COUNTER(MG_Gesture_Counter,CClipDictionaryMemoryGroupsStats);
	PF_COUNTER(MG_Melee_Counter,CClipDictionaryMemoryGroupsStats);
	PF_COUNTER(MG_Misc_Counter,CClipDictionaryMemoryGroupsStats);
	PF_COUNTER(MG_Parachuting_Counter,CClipDictionaryMemoryGroupsStats);
	PF_COUNTER(MG_PlayerMovement_Counter,CClipDictionaryMemoryGroupsStats);
	PF_COUNTER(MG_ScenariosBase_Counter,CClipDictionaryMemoryGroupsStats);
	PF_COUNTER(MG_ScenariosStreamed_Counter,CClipDictionaryMemoryGroupsStats);
	PF_COUNTER(MG_Script_Counter,CClipDictionaryMemoryGroupsStats);
	PF_COUNTER(MG_ShockingEventReactions_Counter,CClipDictionaryMemoryGroupsStats);
	PF_COUNTER(MG_Swimming_Counter,CClipDictionaryMemoryGroupsStats);
	PF_COUNTER(MG_VaultingClimbingJumping_Counter,CClipDictionaryMemoryGroupsStats);
	PF_COUNTER(MG_Vehicle_Counter,CClipDictionaryMemoryGroupsStats);
	PF_COUNTER(MG_Weapons_Counter,CClipDictionaryMemoryGroupsStats);
};
using namespace CAnimAssocProfile;
#endif //ANIMASSOC_STATS

bool CAnimViewer::m_showAnimMemory = false;
bool CAnimViewer::m_showAnimDicts = false;

bool CAnimViewer::m_showPercentageOfMGColumn = true;
bool CAnimViewer::m_showPercentageOfMGBudgetColumn = true;
bool CAnimViewer::m_showClipCountColumn = true;
bool CAnimViewer::m_showLoadedColumn = true;
bool CAnimViewer::m_showPolicyColumn = true;
bool CAnimViewer::m_showReferenceCountColumn = true;
bool CAnimViewer::m_showMemoryGroupColumn = true;
bool CAnimViewer::m_showCompressionColumn = true;

bool CAnimViewer::m_isLoaded = false;
bool CAnimViewer::m_isNotLoaded = false;
bool CAnimViewer::m_isResident = false;
bool CAnimViewer::m_isStreaming = false;
bool CAnimViewer::m_haveReferences	= true;
bool CAnimViewer::m_haveNoReferences	= false;

bool CAnimViewer::m_sortByName = true;
bool CAnimViewer::m_sortBySize = false;

bool CAnimViewer::m_showAnimLODInfo = false;
bool CAnimViewer::m_showAnimPoolInfo = false;

bool CAnimViewer::m_bLogAnimDictIndexAndHashKeys = false;

int CAnimViewer::m_startLine = 0;
int CAnimViewer::m_endLine = CAnimViewer::MAX_LINES_IN_TABLE;
int CAnimViewer::m_selectedCount = 0;

atVector<CAnimViewer::sAnimDictSummary> CAnimViewer::m_selectedAnimDictSummary;


//////////////////////////////////////////////////////////////////////////
//	Sorting classes
//////////////////////////////////////////////////////////////////////////

class AnimNameGroupSizeNameSort
{
public:
	bool operator()(const atVector<CAnimViewer::sAnimDictSummary>::value_type left, const atVector<CAnimViewer::sAnimDictSummary>::value_type right)
	{
		u32 i;
		for(i = 0; i< strlen(left.pAnimDictName); i++)
		{
			if( i >= strlen(right.pAnimDictName) )
			{
				return true;
			}
			if( toupper(left.pAnimDictName[i] ) != toupper(right.pAnimDictName[i] ) )
			{
				return toupper(left.pAnimDictName[i] ) < toupper(right.pAnimDictName[i] );
			}
		}
		return false;
	}
};

class AnimNameGroupSizeSizeSort
{
public:
	bool operator()(const atVector<CAnimViewer::sAnimDictSummary>::value_type left, const atVector<CAnimViewer::sAnimDictSummary>::value_type right)
	{
		return (left.animDictSize > right.animDictSize);
	}
};

// Pm sorting function
class PmPred
{
public:
	bool operator()(const atVector<CAnimViewer::sPmDictionary>::value_type left, const atVector<CAnimViewer::sPmDictionary>::value_type right)
	{
		return strcmp(left.p_name.GetCStr(), right.p_name.GetCStr()) < 0;
	}
};

// PURPOSE: Iterates clip degrees of freedom against those in skeleton data
// Records bones that are present in clip in three arrays (m_Translation, m_Rotation and m_Scale)
class ClipBoneDofIterator : crFrameIterator<ClipBoneDofIterator>
{
public:

	ClipBoneDofIterator(const crClip& clip, const crSkeletonData& skelData)
		: crFrameIterator<ClipBoneDofIterator>(m_Frame)
		, m_NumDofs(0)
	{
		clip.InitDofs(m_FrameData);
		m_Frame.Init(m_FrameData);

		const int numBones = skelData.GetNumBones();

		m_Translation.Reset();
		m_Translation.Resize(numBones);

		m_Rotation.Reset();
		m_Rotation.Resize(numBones);

		m_Scale.Reset();
		m_Scale.Resize(numBones);


		for(int i=0; i<numBones; ++i)
		{
			m_Translation[i] = false;
			m_Rotation[i] = false;
			m_Scale[i] = false;
		}

		m_NumDofs = 0;

		crFrameIterator<ClipBoneDofIterator>::IterateSkel(skelData, NULL, 1.f, false);
	}

	void IterateDofBone(const crFrameData::Dof& dof, crFrame::Dof&, int boneIdx, float)
	{
		switch(dof.m_Track)
		{
		case kTrackBoneTranslation:
			m_Translation[boneIdx] = true;
			m_NumDofs++;
			break;

		case kTrackBoneRotation:
			m_Rotation[boneIdx] = true;
			m_NumDofs++;
			break;

		case kTrackBoneScale:
			m_Scale[boneIdx] = true;
			m_NumDofs++;
			break;
		}
	}

	datPadding<12> m_Pad;

	crFrameData m_FrameData;
	crFrame m_Frame;

	atArray<bool> m_Translation;
	atArray<bool> m_Rotation;
	atArray<bool> m_Scale;

	int m_NumDofs;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
// CAnimViewer
///////////////////////////////////////////////////////////////////////////////////////////////////
void CAnimViewer::InitBoneNames()
{
	m_boneNames.clear();

	if (m_pDynamicEntity && m_pDynamicEntity->GetSkeleton())
	{

		animAssert(m_pDynamicEntity);

		const int numBones = m_pDynamicEntity->GetSkeletonData().GetNumBones();
		
		for(int i=0; i<numBones; ++i)
		{
			m_boneNames.Grow() = "";
		}

		for (s32 i=0; i<NUM_BONE_COMBOS; i++)
		{
			if (m_pBoneCombo[i])
			{
				m_pBoneCombo[i]->UpdateCombo("Select Bone", m_pBoneCombo[i]->GetValuePtr(), m_boneNames.GetCount(), &m_boneNames[0], SelectBone);
			}
		}

		m_boneNames.clear();
		for(int i=0; i<numBones; ++i)
		{
			m_boneNames.Grow() = m_pDynamicEntity->GetSkeletonData().GetBoneData(i)->GetName();
		}

		for (s32 i=0; i<NUM_BONE_COMBOS; i++)
		{
			if (m_pBoneCombo[i])
			{
				for (int j = 0; j < m_boneNames.GetCount(); ++j)
					m_pBoneCombo[i]->SetString(j, m_boneNames[j]);
			}
		}


		SelectBone();
	}

}

void CAnimViewer::InitSpringNames()
{
	m_springNames.clear();

	if (m_pDynamicEntity && m_pDynamicEntity->GetCreature())
	{
		crCreature& creature = *m_pDynamicEntity->GetCreature();

		crCreatureComponentPhysical* physicalComponent = creature.FindComponent<crCreatureComponentPhysical>();
		if (physicalComponent != NULL)
		{
			const crSkeletonData& skelData = m_pDynamicEntity->GetSkeletonData();
			const crCreatureComponentPhysical::Motion* motions = physicalComponent->GetMotions();
			int motionCount = physicalComponent->GetNumMotions();

			for (int i=0; i<motionCount; ++i) 
			{
				m_springNames.Grow() = "";
			}

			m_springNames.clear();
			for (int i=0; i<motionCount; ++i)
			{
				const crCreatureComponentPhysical::Motion& motion = motions[i];
				int boneIdx;
				if (skelData.ConvertBoneIdToIndex(motion.m_Descr.GetBoneId(), boneIdx))
				{
					m_springNames.Grow() = skelData.GetBoneData(boneIdx)->GetName();
				}
			}
		}

		SelectSpring();
	}

	if(m_pSpringBoneCombo)
	{
		if (m_springNames.GetCount() )
		{
			for (int i=0, springCount=m_springNames.GetCount(); i<springCount; ++i)
			{
				m_pSpringBoneCombo->SetString(i, m_springNames[i]);
			}
		}
		else
		{
			m_springIndex = 0;
			m_springNames.Grow() = "None";
		}

		m_pSpringBoneCombo->UpdateCombo("Select spring", &m_springIndex, m_springNames.GetCount(), &m_springNames[0], SelectSpring);
	}
}

#if ENABLE_BLENDSHAPES
void CAnimViewer::InitBlendshapeNames()
{
	if(!m_pDynamicEntity || !m_pDynamicEntity->GetIsTypePed())
		return;

	m_blendShapeIndex = 0;
	m_blendShapeNames.clear();
	m_blendShapeBlends.clear();

	CPed* pPed = static_cast<CPed*>(m_pDynamicEntity.Get());
	if (!pPed)
	{
		return;
	}

	rmcDrawable* pHeadComponent = pPed->GetComponent(PV_COMP_HEAD);
	bool bBlendshapes = false;

	if (pHeadComponent)  
	{
		grbTargetManager* pBlendShapeManager = m_pDynamicEntity->GetTargetManager();
		if(pBlendShapeManager)
		{
			atMap<u16, datOwner<grbTarget> >::ConstIterator iter = pBlendShapeManager->GetBlendTargetIterator();
			while(iter)
			{
				const grbTarget *pTarget = iter.GetData();
				if (pTarget)
				{
					bBlendshapes = true;
					m_blendShapeNames.Grow() = "";
					m_blendShapeBlends.Grow() = 0.0f;
				}
				++iter;
			}
		}
	}

	if (!bBlendshapes)
	{
		m_blendShapeNames.Grow() = "Empty";
	}

	if (m_pBlendshapeCombo)
	{
		m_pBlendshapeCombo->UpdateCombo("Select Blendshape", &m_blendShapeIndex, m_blendShapeNames.GetCount(), &m_blendShapeNames[0], SelectBlendShape);
	}

	m_blendShapeNames.clear();

	if (pHeadComponent)
	{
		grbTargetManager* pBlendShapeManager = m_pDynamicEntity->GetTargetManager();
		if(pBlendShapeManager)
		{
			atMap<u16, datOwner<grbTarget> >::ConstIterator iter = pBlendShapeManager->GetBlendTargetIterator();
			while(iter)
			{
				const grbTarget *pTarget = iter.GetData();
				if (pTarget)
				{
					m_blendShapeNames.Grow() = pTarget->GetName();
				}
				++iter;
			}
		}
	}

	if (m_pBoneCombo[0])
	{
		if (pHeadComponent)
		{
			grbTargetManager* pBlendShapeManager = m_pDynamicEntity->GetTargetManager();;
			if(pBlendShapeManager)
			{
				int i=0;
				atMap<u16, datOwner<grbTarget> >::ConstIterator iter = pBlendShapeManager->GetBlendTargetIterator();
				while(iter)
				{
					const grbTarget *pTarget = iter.GetData();
					if (pTarget)
					{
						m_pBlendshapeCombo->SetString(i, m_blendShapeNames[i]);
						++i;
					}
					++iter;
				}
			}
		}
	}

	SelectBlendShape();
}
#endif // ENABLE_BLENDSHAPES

void CAnimViewer::InitModelNameCombo()
{
	m_modelStrings.clear();
	m_modelNames.clear();

	fwArchetypeDynamicFactory<CPedModelInfo>& pedModelInfoStore = CModelInfo::GetPedModelInfoStore();
	atArray<CPedModelInfo*> pedTypeArray;
	pedModelInfoStore.GatherPtrs(pedTypeArray);
	int numPedsAvail = pedTypeArray.GetCount();

	for (int i=0; i<numPedsAvail; i++)
	{
		CPedModelInfo& pedModelInfo = *pedTypeArray[i];

		bool allowCutscenePeds = false;
#if __DEV
		// Force the player to start in the world if that parameter is set
		XPARAM(allowCutscenePeds);
		if( PARAM_allowCutscenePeds.Get() )
		{
			allowCutscenePeds = true;
		}
#endif //__DEV

		if((strncmp(pedModelInfo.GetModelName(), "CS_", 3) != 0) || allowCutscenePeds)
		{
			// Add the file name to the array of model names
			m_modelStrings.push_back(atString(pedModelInfo.GetModelName()));
		}
	}

	// Get the model names into in a format that can be used with AddCombo
	for (atVector<atString>::iterator nit = m_modelStrings.begin();
		nit != m_modelStrings.end(); ++nit)
	{
		nit->Lowercase();
		m_modelNames.Grow() = *nit;
	}

	// Sort the model names alphabetically
	std::sort(&m_modelNames[0], &m_modelNames[0] + m_modelNames.GetCount(), AlphabeticalSort());

	if (m_pDynamicEntity)
	{
		// Set the model index to point at the player model
		for (int i=0; i<m_modelNames.GetCount(); i++)
		{
			u32 modelIndex = CModelInfo::GetModelIdFromName(m_modelNames[i]).GetModelIndex();
			if (modelIndex  == m_pDynamicEntity->GetModelId().GetModelIndex())
			{
				m_modelIndex = i;
			}
		}
	}
}

#if USE_PM_STORE
void CAnimViewer::InitPmDictNameCombo()
{
	// For each pm dictionary
	// we check if the associated pm block is in the image
	// if successful we try and stream in the pm block
	// if successful we iterate over the individual entries in
	// the pm dictionary

	m_pmDictionary.clear();
	m_pmDictionaryNames.clear();

	int pmDictionaryCount = g_PmDictionaryStore.GetSize();
	for (int i = 0; i<pmDictionaryCount; i++)
	{
		// Is the current slot valid
		if (g_PmDictionaryStore.IsValidSlot(i))
		{
			//const char* p_pmDictionaryName = g_PmDictionaryStore.GetName(i);
			//Displayf("%s\n", p_pmDictionaryName);

			// Is the associated pm block in the image?
			if (g_PmDictionaryStore.IsObjectInImage(i))
			{
				//Displayf("Object is in image %s\n", p_pmDictionaryName);
				// Try and stream in the pm block
				g_PmDictionaryStore.StreamingRequest(i, STRFLAG_FORCE_LOAD);
				CStreaming::LoadAllRequestedObjects();
			}
			else
			{
				//Displayf("Object is not in image %s\n", p_pmDictionaryName);
			}

			// Is the pm dictionary streamed in?
			if (g_PmDictionaryStore.HasObjectLoaded(i))
			{
				//Displayf("Object has loaded %s\n", p_pmDictionaryName);

				// Enumerate each of the pms in the pm block
				const char* p_dictionaryName = g_PmDictionaryStore.GetName(i);
				sPmDictionary pmGroup;
				m_pmDictionary.push_back(pmGroup);
				atVector<CAnimViewer::sPmDictionary>::reference group = m_pmDictionary.back();
				group.p_name = strStreamingObjectName(p_dictionaryName);

				crpmParameterizedMotionDictionary* pPmDict = g_PmDictionaryStore.Get(i);
				pPmDict->ForAll(EnumeratePm, (void*)&group.pmStrings);

				const int numPms = pPmDict->GetNumParameterizedMotions();
				if (numPms==0)
				{
					Warningf("%s has no pms \n\n", p_dictionaryName);
					static atString s_emptyString("empty\0");
					group.pmStrings.push_back(s_emptyString);
				}

				// Get the filenames into in a format that can be used with AddCombo
				for (atVector<atString>::const_iterator nit = group.pmStrings.begin();
					nit != group.pmStrings.end(); ++nit)
				{
					group.pmNames.Grow() = *nit;
				}
			}
			else
			{
				//Displayf("Object has not loaded %s\n", p_pmDictionaryName);
			}
		}
	}

	if (m_pmDictionary.size() > 0)
	{
		// Sort the pm dictionary names alphabetically
		std::sort(&m_pmDictionary[0], &m_pmDictionary[0] + m_pmDictionary.size(), PmPred());

		// Get the filenames into in a format that can be used with AddCombo
		for (atVector<sPmDictionary>::iterator group = m_pmDictionary.begin();
			group != m_pmDictionary.end(); ++group)
		{
			m_pmDictionaryNames.Grow() = group->p_name.GetCStr();

			int k=0;

			// Get the filenames into in a format that can be used with AddCombo
			for (atVector<atString>::iterator nit = group->pmStrings.begin();
				nit != group->pmStrings.end(); ++nit)
			{
				nit->Lowercase();
				group->pmNames[k] = *nit;
				k++;
			}

			if (group->pmNames.GetCount()>0)
			{
				// Sort the pm names alphabetically
				std::sort(&group->pmNames[0], &group->pmNames[0] + group->pmNames.GetCount(), AlphabeticalSort());
			}
		}
	}
}
#endif // USE_PM_STORE


void CAnimViewer::InitBoneAndMoverHistory()
{
	m_moverTrackTransform.Identity();
}

void CAnimViewer::Init()
{
	InternalInitialise();

	//Initialise our other anim debug systems

	CAnimClipEditor::Initialise();

	CGestureEditor::Initialise();

	fwClipSetManager::InitWidgets();

	m_fVertDist = 0.015f;
}

void CAnimViewer::InternalInitialise()
{
	if (!m_pDynamicEntity)
	{
		// Cache a pointer to the player
		m_pDynamicEntity = FindPlayerPed();
		animAssert(m_pDynamicEntity);
	}

	for (s32 i=0; i<MAX_DOFS_TO_TRACK; i++)
	{
		ms_DofTrackerBoneIndices[i] = i;
	}

	for (s32 i=0; i<NUM_BONE_COMBOS; i++)
	{
		m_pBoneCombo[i] = NULL;
	}

	m_bCaptureMotionTree = PARAM_capturemotiontree.Get();
	m_bCaptureDofs = m_bCaptureMotionTree;

	m_BlendOutDataEnabled = PARAM_blendoutdataenabled.Get();

	Assert(!m_MoveDump);
	m_MoveDump = rage_new fwMoveDumpFrameSequence(GetMotionTreeCaptureCount());

	m_startPosition = VEC3V_TO_VECTOR3(m_pDynamicEntity->GetTransform().GetPosition());

	ToggleUninterruptableTask();

	InitBoneAndMoverHistory();
}

void CAnimViewer::Shutdown(unsigned /*shutdownMode*/)
{
	CAnimClipEditor::Shutdown();

	CGestureEditor::Shutdown();

	InternalShutdown();
}

void CAnimViewer::InternalShutdown()
{
	delete m_MoveDump; m_MoveDump = NULL;

	m_pDynamicEntity = NULL;

	ClearBlendOutData();

	m_BlendOutDataGroup = NULL;

	// Shutdown the debug skeleton array
	m_debugSkeletons.clear();

	m_pLinearPhysicalExpressionGroup = NULL;
	m_pSpringBoneCombo = NULL;

	// Pointer to the bone name combo
	for (s32 i=0; i<NUM_BONE_COMBOS; i++)
	{
		m_pBoneCombo[i] = NULL;
	}
	

	m_pmDictionary.clear();
	m_pmDictionaryNames.clear();

#if ENABLE_BLENDSHAPES
	m_blendShapeNames.clear();
#endif // ENABLE_BLENDSHAPES
	m_boneNames.clear();
	m_modelNames.clear();
	m_modelStrings.clear();

	m_selectedAnimDictSummary.clear();

	// Allow ambient anims again
	CTaskAmbientClips::BLOCK_ALL_AMBIENT_IDLES = false;
}

#if __BANK	
void CAnimViewer::OnSelectingCutSceneEntity()
{
	CutSceneManager* pManager = CutSceneManager::GetInstance(); 
	if (pManager->IsRunning())
	{
		RegdDyn pDynamicEntity; 

		CCutsceneAnimatedActorEntity* pActor =  pManager->GetDebugManager().BankGetAnimatedActorEntity(); 
		CCutSceneAnimatedPropEntity* pProp = pManager->GetDebugManager().BankGetAnimatedPropEntity(); 
		
		if(pActor)
		{
			if(pActor->GetGameEntity())
			{
				CDynamicEntity* DynamicEntity  = static_cast<CDynamicEntity*>(pActor->GetGameEntity()); 
				pDynamicEntity = DynamicEntity; 
			}
		}
		
		if(pProp)
		{
			if(pProp->GetGameEntity())
			{
				CDynamicEntity* DynamicEntity  = static_cast<CDynamicEntity*>(pProp->GetGameEntity()); 
				pDynamicEntity = DynamicEntity; 
			}
		}

		if (pDynamicEntity && m_pDynamicEntity!=pDynamicEntity)
		{
			
			//remove any existing MoVE preview parameter widgets
			RemoveParamWidgetsOnEntity(m_pDynamicEntity);

			m_pDynamicEntity = pDynamicEntity;

			// Lazily create the animBlender if it doesn't exist
			if (!m_pDynamicEntity->GetAnimDirector()) 
			{
				m_pDynamicEntity->CreateAnimDirector(*m_pDynamicEntity->GetDrawable());
			}

			// The skeleton may have changed re-init the bone & spring names
			InitBoneNames();
			InitSpringNames();

			// The skeleton may have changed re-init the blendshape names
			//InitBlendshapeNames();

			// Add any MoVE preview parameter widgets to the Animation preview section
			AddParamWidgetsFromEntity(m_pDynamicEntity);
		}
	}
}
#endif // __BANK

void CAnimViewer::SetSelectedEntity(CDynamicEntity* pDynamicEntity)
{
	if (m_pDynamicEntity!=pDynamicEntity)
	{
		//remove any existing MoVE preview parameter widgets
		RemoveParamWidgetsOnEntity(m_pDynamicEntity);

		m_pDynamicEntity = pDynamicEntity;

		if (m_active)
		{
			// Lazily create the drawable if it doesn't exist
			if(!m_pDynamicEntity->GetDrawable())
			{
				m_pDynamicEntity->CreateDrawable();
			}

			if (m_pDynamicEntity->GetDrawable())
			{
				// Lazily create the skeleton if it doesn't exist
				if (!m_pDynamicEntity->GetSkeleton())
				{
					m_pDynamicEntity->CreateSkeleton();
				}

				if (m_pDynamicEntity->GetSkeleton())
				{

					if(m_pDynamicEntity->GetIsTypeObject())
					{
						CObject * pObject = static_cast<CObject *>(m_pDynamicEntity.Get());
						pObject->InitAnimLazy();
					}

					// Lazily create the animBlender if it doesn't exist
					if (!m_pDynamicEntity->GetAnimDirector())
					{
						m_pDynamicEntity->CreateAnimDirector(*m_pDynamicEntity->GetDrawable());
					}

					if (m_pDynamicEntity->GetAnimDirector())
					{
						// The skeleton may have changed re-init the bone & spring names
						InitBoneNames();
						InitSpringNames();

						// Add any MoVE preview parameter widgets to the Animation preview section
						AddParamWidgetsFromEntity(m_pDynamicEntity);
					}
				}

			}
		
		}
	}
}

void CAnimViewer::FocusEntityChanged(CEntity* pEntity)
{
	if(pEntity && pEntity->GetIsDynamic() )
	{
		// Is the entity a dynamic entity and does it have a skeleton data
		CDynamicEntity* pDynamicEntity = static_cast<CDynamicEntity*>(pEntity);
		if (pDynamicEntity)
		{
			SetSelectedEntity(pDynamicEntity);
		}
	}

	if(!m_pDynamicEntity)
	{
		m_pDynamicEntity = FindPlayerPed();
		animAssert(m_pDynamicEntity);
	}

	if(m_pDynamicEntity && m_pDynamicEntity->GetIsTypePed())
	{
		CPed *pPed = static_cast< CPed * >(m_pDynamicEntity.Get());

		CTaskMotionBase *pTaskMotionBase = pPed->GetPrimaryMotionTask();
		if (pTaskMotionBase)
		{
			fwMvClipSetId motionClipSetId = pTaskMotionBase->GetDefaultOnFootClipSet();
			for(m_motionClipSetIndex = 0; m_motionClipSetIndex < fwClipSetManager::GetClipSetNames().GetCount(); m_motionClipSetIndex ++)
			{
				if(motionClipSetId == fwMvClipSetId(fwClipSetManager::GetClipSetNames()[m_motionClipSetIndex]))
				{
					break;
				}
			}
			if(m_motionClipSetIndex > 0 && m_motionClipSetIndex >= fwClipSetManager::GetClipSetNames().GetCount())
			{
				m_motionClipSetIndex = 0;
			}

			fwMvClipSetId weaponClipSetId = pTaskMotionBase->GetOverrideWeaponClipSet();
			for(m_weaponClipSetIndex = 0; m_weaponClipSetIndex < fwClipSetManager::GetClipSetNames().GetCount(); m_weaponClipSetIndex ++)
			{
				if(weaponClipSetId == fwMvClipSetId(fwClipSetManager::GetClipSetNames()[m_weaponClipSetIndex]))
				{
					break;
				}
			}
			if(m_weaponClipSetIndex > 0 && m_weaponClipSetIndex >= fwClipSetManager::GetClipSetNames().GetCount())
			{
				m_weaponClipSetIndex = 0;
			}
		}

		CFacialDataComponent *pFacialDataComponent = pPed->GetFacialData();
		if(pFacialDataComponent)
		{
			fwMvClipSetId facialClipSetId = pFacialDataComponent->GetFacialClipSet();
			for(m_facialClipSetIndex = 0; m_facialClipSetIndex < fwClipSetManager::GetClipSetNames().GetCount(); m_facialClipSetIndex ++)
			{
				if(facialClipSetId == fwMvClipSetId(fwClipSetManager::GetClipSetNames()[m_facialClipSetIndex]))
				{
					break;
				}
			}
			if(m_facialClipSetIndex > 0 && m_facialClipSetIndex >= fwClipSetManager::GetClipSetNames().GetCount())
			{
				m_facialClipSetIndex = 0;
			}
		}

		fwAnimDirectorComponentFacialRig *pAnimDirectorComponentFacialRig = static_cast< fwAnimDirectorComponentFacialRig * >(pPed->GetAnimDirector()->GetComponentByType(fwAnimDirectorComponent::kComponentTypeFacialRig));
		if(pAnimDirectorComponentFacialRig)
		{
			m_bEnableFacial = pAnimDirectorComponentFacialRig->GetEnable();
		}
	}
	else
	{
		m_motionClipSetIndex = 0;
		m_weaponClipSetIndex = 0;
		m_facialClipSetIndex = 0;
		m_bEnableFacial = true;
	}
}

void CAnimViewer::OnPressedLeftMouseButton()
{
	// On pressing the left mouse button
	if((ioMouse::GetPressedButtons()&ioMouse::MOUSE_LEFT))
	{
		fwEntity* pEnt = g_PickerManager.GetSelectedEntity();
		CAnimViewer::FocusEntityChanged((CEntity* )pEnt);
	}
}

void CAnimViewer::OnHoldingDownLeftMouseButton()
{
	// On holding down the right mouse button
	if (ioMouse::GetButtons()&ioMouse::MOUSE_LEFT)
	{
	}
}

void CAnimViewer::OnReleasedLeftMouseButton()
{
	// On releasing the right mouse button
	if((ioMouse::GetReleasedButtons()&ioMouse::MOUSE_LEFT))
	{
	}
}

void CAnimViewer::OnPressedRightMouseButton()
{

}

void CAnimViewer::OnHoldingDownRightMouseButton()
{

}

void CAnimViewer::OnReleasedRightMouseButton()
{

}

void CAnimViewer::UpdateSelectedDynamicEntity()
{
	if (!m_pDynamicEntity)
	{
		return;
	}

	if (m_bLockPosition || m_bLockPosition || m_bResetMatrix)
	{
		if(m_bLockRotation)
		{
			Matrix34 matSet;
			matSet.Identity();
			matSet.d = VEC3V_TO_VECTOR3(m_pDynamicEntity->GetTransform().GetPosition());
			m_pDynamicEntity->SetMatrix(matSet, true, true, true);
		}

		if (m_bLockPosition)
		{
			m_pDynamicEntity->SetPosition(m_startPosition, true, true, true);
		}

		if(m_bResetMatrix)
		{
			Matrix34 matSet;
			matSet.Identity();
			static Vector3 resetPos(0.0f, 0.0f, 1.0f);
			matSet.d = resetPos;
			m_pDynamicEntity->SetMatrix(matSet, true, true, true);
		}

		if (m_pDynamicEntity->GetIsTypePed())
		{
			CPed* pPed = static_cast<CPed*>(m_pDynamicEntity.Get());
			pPed->SetVelocity(Vector3(0.0f, 0.0f, 0.0f));
			pPed->SetAngVelocity(Vector3(0.0f, 0.0f, 0.0f));
			pPed->SetIsStanding(true);
		}
	}
}

void CAnimViewer::UpdateSelectedBone()
{
	/*
	// Get the rotation for the given bone index
	crBone &bone = m_pDynamicEntity->GetSkeleton()->GetBone(m_boneIndex);
	bone.GetLocalMtx().FromEulersXYZ(m_rotation);
	m_pDynamicEntity->GetSkeleton()->Update();

	Matrix34 newLocalMat;
	newLocalMat.FromEulersXYZ(m_rotation);
	Matrix34& localMat = m_pDynamicEntity->GetSkeleton()->GetBone(m_boneIndex).GetLocalMtx();
	static Vector3 temp = localMat.d;
	localMat.d = temp;
	m_pDynamicEntity->GetSkeleton()->Update();

	// Keep the bone index in range
	int numberOfBones = m_pDynamicEntity->GetSkeletonData().GetNumBones();
	numberOfBones--;
	if (m_boneIndex>numberOfBones)
	{
	m_boneIndex = 0;
	}
	if (m_boneIndex<0)
	{
	m_boneIndex = numberOfBones;
	}
	*/
}

void CAnimViewer::EnableDebugRecorder()
{
#if DR_ENABLED
	if (!fwTimer::IsUserPaused() && !fwTimer::IsGamePaused())
	{
		debugPlayback::phInstId entityId(m_pDynamicEntity->GetCurrentPhysicsInst());
		if ((debugPlayback::GetCurrentSelectedEntity()!=entityId) || (!GameRecorder::GetInstance()->IsRecording()))
		{
			debugPlayback::DR_EventType::DisableAll();

			//Enable tasks useful for animation debugging
			debugPlayback::DR_EventType::Enable("SetTaggedStringCollectionEvent");
			debugPlayback::DR_EventType::Enable("CTaskFlowDebugEvent");

			debugPlayback::DR_EventType::Enable("MoveTransitionStringCollection");
			debugPlayback::DR_EventType::Enable("MoveConditionStringCollection");
			debugPlayback::DR_EventType::Enable("MoveSetValueStringCollection");

			debugPlayback::DR_EventType::Enable("MoveGetValue");
			debugPlayback::DR_EventType::Enable("MoveSetValue");
			debugPlayback::DR_EventType::Enable("StoreSkeletonPostUpdate");
			debugPlayback::DR_EventType::Enable("EntityAttachmentUpdate");
			debugPlayback::DR_EventType::Enable("PedDesiredVelocityEvent");			
			debugPlayback::DR_EventType::Enable("PedAnimatedVelocityEvent");

			debugPlayback::SetCurrentSelectedEntity(m_pDynamicEntity->GetCurrentPhysicsInst(), true);
			debugPlayback::SetTrackMode(debugPlayback::eTrackOne);
			GameRecorder::GetInstance()->StartRecording();
		}
	}
#endif //DR_ENABLED
}

void CAnimViewer::Process()
{
#if __BANK
	OnSelectingCutSceneEntity(); 
#endif // __BANK

	OnPressedLeftMouseButton();

	if (m_bNeedsParamWidgetOptionsRebuild)
	{
		RebuildParamWidgetOptions();
	}

	CAnimClipEditor::Update();

	CGestureEditor::Update();
	
	if (!m_active)
		return;

	EntityCaptureUpdate();

	UpdateSelectedBone();

	UpdateSelectedDynamicEntity();

	AutoAddDebugSkeleton();

	UpdateExpressionDebugWidgets();

	UpdatePhase(); 

	UpdateClipSetVariables();

	UpdateAnimatedBuilding();

	UpdatePreviewCamera();

	if (ms_DofTrackerUpdatePostPhysics)
	{
		UpdateDOFTrackingForEKG();
	}

	if (m_bTrackLargestMotionTree && (!fwTimer::IsUserPaused() || fwTimer::GetSingleStepThisFrame()) && !fwTimer::IsGamePaused())
	{
		// run over the peds and select the one with the most complex motion tree
		CPed::Pool *PedPool = CPed::GetPool();
		CPed* pPed;
		s32 i=PedPool->GetSize();
		s32 lastMax = 0;
		s32 lastMaxDepth = 0;
		while(i--)
		{
			pPed = PedPool->GetSlot(i);
			if(pPed)
			{
				fwMotionTreeTester it;
				fwMove& move = pPed->GetAnimDirector()->GetMove();
				crmtMotionTree& tree = move.GetMotionTree();
				tree.Iterate(it);

				if (it.m_complexity>lastMax || it.m_MaxDepth>lastMaxDepth)
				{
					m_pDynamicEntity = pPed;
					m_bCaptureMotionTree = true;
					g_PickerManager.ResetList(true);
					g_PickerManager.AddEntityToList(pPed, false, false);
					EnableDebugRecorder();

					if (it.m_complexity>lastMax)
					{
						lastMax = it.m_complexity;
					}
					if(it.m_MaxDepth>lastMaxDepth)
					{
						lastMaxDepth = it.m_MaxDepth;
					}

					if (it.m_complexity>fwMotionTreeTester::ms_maxComplexity)
					{
						fwMotionTreeTester::ms_maxComplexity = it.m_complexity;
						animDebugf1("***New high watermark in motiontree complexity: %d nodes, ped:%s***", fwMotionTreeTester::ms_maxComplexity, pPed->GetModelName());
						if (fwMotionTreeTester::ms_maxComplexity>fwMotionTreeTester::ms_autoPauseComplexity )
						{
							fwTimer::StartUserPause();
						}
					}
					if (it.m_MaxDepth>fwMotionTreeTester::ms_maxTreeDepth)
					{
						fwMotionTreeTester::ms_maxTreeDepth = it.m_MaxDepth;
						animDebugf1("***New high watermark in motiontree depth: %d nodes, ped:%s***", fwMotionTreeTester::ms_maxTreeDepth, pPed->GetModelName());
						if (fwMotionTreeTester::ms_maxTreeDepth>fwMotionTreeTester::ms_autoPauseTreeDepth )
						{
							fwTimer::StartUserPause();
						}

					}
				}
			}
		}	
	}


#if CRMT_NODE_2_DEF_MAP
	if(PARAM_movedebug.Get())
	{
		UpdateNetworkNodePropertyData();
	}
#endif //CRMT_NODE_2_DEF_MAP
}

void CAnimViewer::AutoAddDebugSkeleton()
{
	//add a debug skeleton if necessary
	if (m_autoAddSkeletonEveryFrame)
	{
		AddDebugSkeleton(Color_green, "Anim viewer (Auto add)");
	}
}


Vec2V CAnimViewer::ConvertPointOnCurveToScreenSpace(Vec2V point)
{
	Vec2V screenSpacePoint(point.GetX(), ScalarV(1.0f)-point.GetY());
	screenSpacePoint = Scale(screenSpacePoint, ScalarV(ms_fRenderCameraCurveScale));
	screenSpacePoint = Add(screenSpacePoint, Vec2V(ScalarV(ms_fRenderCameraCurveOffsetX), ScalarV(ms_fRenderCameraCurveOffsetY)));
	return screenSpacePoint;
};

float CAnimViewer::ApplyCustomBlendCurve(float sourceLevel, s32 curveType, bool shouldBlendIn)
{
	float blendLevel = 0.0f;

	if (!shouldBlendIn) 
	{ 
		sourceLevel = 1.0f - sourceLevel; 
	}

	switch(curveType)
	{
	case CURVE_TYPE_EXPONENTIAL:
		{
			//Make the release an exponential (rather than linear) decay, as this looks better.
			//NOTE: exp(-7x) gives a result of less than 0.001 at x=1.0.
			blendLevel = exp(-7.0f * (1.0f - sourceLevel));
		}
		break;

	case CURVE_TYPE_SLOW_IN:
		{
			blendLevel = SlowIn(sourceLevel);
		}
		break;

	case CURVE_TYPE_SLOW_OUT:
		{
			blendLevel = SlowOut(sourceLevel);
		}
		break;

	case CURVE_TYPE_SLOW_IN_OUT:
		{
			blendLevel = SlowInOut(sourceLevel);
		}
		break;

	case CURVE_TYPE_VERY_SLOW_IN:
		{
			blendLevel = SlowIn(SlowIn(sourceLevel));
		}
		break;

	case CURVE_TYPE_VERY_SLOW_OUT:
		{
			blendLevel = SlowOut(SlowOut(sourceLevel));
		}
		break;

	case CURVE_TYPE_VERY_SLOW_IN_SLOW_OUT:
		{
		    blendLevel = SlowIn(SlowInOut(sourceLevel));
		}
		break;

	case CURVE_TYPE_SLOW_IN_VERY_SLOW_OUT:
		{
			blendLevel = SlowOut(SlowInOut(sourceLevel));
		}
		break;

	case CURVE_TYPE_VERY_SLOW_IN_VERY_SLOW_OUT:
		{
			blendLevel = SlowInOut(SlowInOut(sourceLevel));
		}
		break;

	case CURVE_TYPE_EASE_IN:
		{
			blendLevel = camBaseSplineCamera::EaseOut(sourceLevel);
		}
		break;

	case CURVE_TYPE_EASE_OUT:
		{
			blendLevel = camBaseSplineCamera::EaseIn(sourceLevel);
		}
		break;

	case CURVE_TYPE_QUADRATIC_EASE_IN:
		{
			//blendLevel =  sourceLevel * sourceLevel;
			blendLevel = QuadraticEaseIn(sourceLevel);
		}
		break;

	case CURVE_TYPE_QUADRATIC_EASE_OUT:
		{
			//float oneMinusSourceLevel = 1.0f - sourceLevel;
			//blendLevel =  1.0f - oneMinusSourceLevel * oneMinusSourceLevel;
			blendLevel = QuadraticEaseOut(sourceLevel);
		}
		break;

	case CURVE_TYPE_QUADRATIC_EASE_IN_OUT:
		{		
			//float phase = sourceLevel;
			//if (phase<0.5)
			//{
			//	float doublePhase = 2.0f * phase;
			//	blendLevel = (doublePhase*doublePhase)/2.0f;
			//}
			//else
			//{
			//	float oneMinusSourcePhase = 1.0f - phase;
			//	float doubleOneMinusSourcePhase = 2.0f * oneMinusSourcePhase;
			//	blendLevel =1.0f - (doubleOneMinusSourcePhase*doubleOneMinusSourcePhase)/2.0f;
			//}
			blendLevel = QuadraticEaseInOut(sourceLevel);
		}
		break;

	case CURVE_TYPE_CUBIC_EASE_IN:
		{
			//blendLevel =  sourceLevel * sourceLevel * sourceLevel;
			blendLevel = CubicEaseIn(sourceLevel);
		}
		break;

	case CURVE_TYPE_CUBIC_EASE_OUT:
		{
			//float oneMinusSourceLevel = 1.0f - sourceLevel;
			//blendLevel =  1.0f - oneMinusSourceLevel * oneMinusSourceLevel * oneMinusSourceLevel;
			blendLevel = CubicEaseOut(sourceLevel);
		}
		break;

	case CURVE_TYPE_CUBIC_EASE_IN_OUT:
		{		
			//float phase = sourceLevel;
			//if (phase<0.5)
			//{
			//	float doublePhase = 2.0f * phase;
			//	blendLevel = (doublePhase*doublePhase*doublePhase)/2.0f;
			//}
			//else
			//{
			//	float oneMinusSourcePhase = 1.0f - phase;
			//	float doubleOneMinusSourcePhase = 2.0f * oneMinusSourcePhase;
			//	blendLevel = 1.0f - (doubleOneMinusSourcePhase*doubleOneMinusSourcePhase* doubleOneMinusSourcePhase)/2.0f;
			//}
			blendLevel = CubicEaseInOut(sourceLevel);
		}
		break;

	case CURVE_TYPE_QUARTIC_EASE_IN:
		{
			//blendLevel =  sourceLevel * sourceLevel * sourceLevel * sourceLevel;
			blendLevel =  QuarticEaseIn(sourceLevel);
		}
		break;

	case CURVE_TYPE_QUARTIC_EASE_OUT:
		{
			//float oneMinusSourceLevel = 1.0f - sourceLevel;
			//blendLevel =  1.0f - oneMinusSourceLevel * oneMinusSourceLevel * oneMinusSourceLevel * oneMinusSourceLevel;
			blendLevel =  QuarticEaseOut(sourceLevel);
		}
		break;
		
	case CURVE_TYPE_QUARTIC_EASE_IN_OUT:
		{
			blendLevel =  QuarticEaseInOut(sourceLevel);
		}
		break;

	case CURVE_TYPE_QUINTIC_EASE_IN:
		{
			blendLevel =  QuinticEaseIn(sourceLevel);
			//blendLevel =  sourceLevel * sourceLevel * sourceLevel * sourceLevel * sourceLevel;
		}
		break;

	case CURVE_TYPE_QUINTIC_EASE_OUT:
		{
			blendLevel =  QuinticEaseOut(sourceLevel);
			//float oneMinusSourceLevel = 1.0f - sourceLevel;
			//blendLevel =  1.0f - oneMinusSourceLevel * oneMinusSourceLevel * oneMinusSourceLevel * oneMinusSourceLevel * oneMinusSourceLevel;
		}
		break;

	case CURVE_TYPE_QUINTIC_EASE_IN_OUT:
		{
			blendLevel =  QuinticEaseInOut(sourceLevel);
		}
		break;

	case CURVE_TYPE_CIRCULAR_EASE_IN:
		{
			blendLevel =  CircularEaseIn(sourceLevel);
			//blendLevel =  1.0f - sqrt(1.0f - (sourceLevel * sourceLevel));
		}
		break;

	case CURVE_TYPE_CIRCULAR_EASE_OUT:
		{
			blendLevel =  CircularEaseOut(sourceLevel);
			//float oneMinusSourceLevel = 1.0f - sourceLevel;
			//blendLevel  = sqrt(1.0f - (oneMinusSourceLevel*oneMinusSourceLevel));
		}
		break;

	case CURVE_TYPE_CIRCULAR_EASE_IN_OUT:
		{
			blendLevel =  CircularEaseInOut(sourceLevel);
		}
		break;

	case CURVE_TYPE_LINEAR:
	default:
		{
			blendLevel = sourceLevel;
		}
	}

	return blendLevel;
}

void CAnimViewer::RenderCameraCurve()
{
	Vec2V p1, p2, p3, p4;
	p1 = Vec2V(0.0, 0.0f); // 
	p2 = Vec2V(1.0, 0.0f); // 
	p3 = Vec2V(1.0, 1.0f); // 
	p4 = Vec2V(0.0, 1.0f); // 

	static float fAlpha = 0.5f;
	// render background quad
	grcDebugDraw::Quad(ConvertPointOnCurveToScreenSpace(p1), ConvertPointOnCurveToScreenSpace(p2), ConvertPointOnCurveToScreenSpace(p3), ConvertPointOnCurveToScreenSpace(p4), Color32(0.0f,0.0f,0.0f, fAlpha));
	
	// render axes
	grcDebugDraw::Line(ConvertPointOnCurveToScreenSpace(p1), ConvertPointOnCurveToScreenSpace(p2), Color32(1.0f,0.0f,0.0f), Color32(1.0f,0.0f,0.0f));

	p1 = Vec2V(0.0, 0.0f);
	p2 = Vec2V(0.0f, 1.0f);
	grcDebugDraw::Line(ConvertPointOnCurveToScreenSpace(p1), ConvertPointOnCurveToScreenSpace(p2), Color32(0.0f,1.0f,0.0f), Color32(0.0f,1.0f,0.0f));	
	
	p1 = Vec2V(0.0f, 0.0f);
	p2 = Vec2V(1.0f, 1.0f);
	grcDebugDraw::Line(ConvertPointOnCurveToScreenSpace(p1), ConvertPointOnCurveToScreenSpace(p2), Color32(0.5f,0.5f,0.5f), Color32(0.5f,0.5f,0.5f));


	// render curve
	for(u32 i = 0; i <= 100; i++)
	{
		p2.SetX(p1.GetX());
		p2.SetY(p1.GetY());
		p1.SetXf((float)i/100.0f);
		p1.SetYf(ApplyCustomBlendCurve(p1.GetXf(), ms_iSelectedCurveType, ms_bShouldBlendIn));

		grcDebugDraw::Line(ConvertPointOnCurveToScreenSpace(p1), ConvertPointOnCurveToScreenSpace(p2), Color32(1.0f,0.0f,0.0f), Color32(1.0f,1.0f,1.0f));
	}

	// render curve name
	p1 = Vec2V(ms_fRenderCameraCurveP1X, ms_fRenderCameraCurveP1Y);
	grcDebugDraw::Text(ConvertPointOnCurveToScreenSpace(p1), Color32(1.0f,1.0f,1.0f), parser_eCurveType_Strings[ms_iSelectedCurveType]);
}

void CAnimViewer::RenderGrid()
{
	// If we are not in the test level?
	Color32 gridColor(0.0f, 0.0f, 1.0f, 0.75f);

	float width = 1.0f;
	int lineCount = 20;
	for(int i=0; i<lineCount; i++)
	{
		float x1 = i * width;
		float x2 = i * width;
		float y1 = 0.0f;
		float y2 = lineCount * width;
		grcDebugDraw::Line(Vector3(  x1,   y1, 0.0f), Vector3(  x2,    y2, 0.0f),  gridColor);
		grcDebugDraw::Line(Vector3( -x1,   y1, 0.0f), Vector3(  -x2,   y2, 0.0f),  gridColor);
		grcDebugDraw::Line(Vector3(  x1,  -y1, 0.0f), Vector3(  x2,   -y2, 0.0f),  gridColor);
		grcDebugDraw::Line(Vector3( -x1,  -y1, 0.0f), Vector3(  -x2,  -y2, 0.0f),  gridColor);

		x1 = 0.0f;
		x2 = lineCount * width;
		y1 = i * width;
		y2 = i * width;
		grcDebugDraw::Line(Vector3(  x1,   y1, 0.0f), Vector3(  x2,    y2, 0.0f),  gridColor);
		grcDebugDraw::Line(Vector3( -x1,   y1, 0.0f), Vector3(  -x2,   y2, 0.0f),  gridColor);
		grcDebugDraw::Line(Vector3(  x1,  -y1, 0.0f), Vector3(  x2,   -y2, 0.0f),  gridColor);
		grcDebugDraw::Line(Vector3( -x1,  -y1, 0.0f), Vector3(  -x2,  -y2, 0.0f),  gridColor);
	}
}

void CAnimViewer::UpdateDOFTrackingForEKG()
{
	// update ekg dof tracking
	if (m_pDynamicEntity && (!fwTimer::IsUserPaused() || fwTimer::GetSingleStepThisFrame()) && !fwTimer::IsGamePaused())
	{
		// mover
		PF_SET(Mover_TransX, m_pDynamicEntity->GetAnimatedVelocity().GetX());
		PF_SET(Mover_TransY, m_pDynamicEntity->GetAnimatedVelocity().GetY());
		PF_SET(Mover_TransZ, m_pDynamicEntity->GetAnimatedVelocity().GetZ());
		PF_SET(Mover_RotZ, m_pDynamicEntity->GetAnimatedAngularVelocity());

		if (m_pDynamicEntity->GetSkeleton())
		{
			// bone 1
			s32 boneIndex = ms_DofTrackerBoneIndices[0];
			if (boneIndex>-1 && boneIndex<m_pDynamicEntity->GetSkeleton()->GetBoneCount())
			{
				Matrix34 boneMat = MAT34V_TO_MATRIX34(m_pDynamicEntity->GetSkeleton()->GetObjectMtx(boneIndex));
				Vector3 eulers;
				boneMat.ToEulers(eulers, "xyz");

				PF_SET(Bone1_TransX, boneMat.d.x);
				PF_SET(Bone1_TransY, boneMat.d.y);
				PF_SET(Bone1_TransZ, boneMat.d.z);
				PF_SET(Bone1_RotX, eulers.x);
				PF_SET(Bone1_RotY, eulers.y);
				PF_SET(Bone1_RotZ, eulers.z);
			}

			// bone 2
			boneIndex = ms_DofTrackerBoneIndices[1];
			if (boneIndex>-1 && boneIndex<m_pDynamicEntity->GetSkeleton()->GetBoneCount())
			{
				Matrix34 boneMat = MAT34V_TO_MATRIX34(m_pDynamicEntity->GetSkeleton()->GetObjectMtx(boneIndex));
				Vector3 eulers;
				boneMat.ToEulers(eulers, "xyz");

				PF_SET(Bone2_TransX, boneMat.d.x);
				PF_SET(Bone2_TransY, boneMat.d.y);
				PF_SET(Bone2_TransZ, boneMat.d.z);
				PF_SET(Bone2_RotX, eulers.x);
				PF_SET(Bone2_RotY, eulers.y);
				PF_SET(Bone2_RotZ, eulers.z);
			}

			// bone 3
			boneIndex = ms_DofTrackerBoneIndices[2];
			if (boneIndex>-1 && boneIndex<m_pDynamicEntity->GetSkeleton()->GetBoneCount())
			{
				Matrix34 boneMat = MAT34V_TO_MATRIX34(m_pDynamicEntity->GetSkeleton()->GetObjectMtx(boneIndex));
				Vector3 eulers;
				boneMat.ToEulers(eulers, "xyz");

				PF_SET(Bone3_TransX, boneMat.d.x);
				PF_SET(Bone3_TransY, boneMat.d.y);
				PF_SET(Bone3_TransZ, boneMat.d.z);
				PF_SET(Bone3_RotX, eulers.x);
				PF_SET(Bone3_RotY, eulers.y);
				PF_SET(Bone3_RotZ, eulers.z);
			}
		}

	}
}

void CAnimViewer::RenderEntityInfo()
{
	if (!m_pDynamicEntity)
	{
		return;
	}

	Color32 colour(Color_red);
	char debugText[100];
	Vector2 vTextRenderPos(m_fHorizontalBorder, m_fVerticalBorder + (m_iNoOfTexts)*m_fVertDist);

	sprintf(debugText, "Entity Pointer %p\n", m_pDynamicEntity.Get());
	vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
	grcDebugDraw::Text(vTextRenderPos, colour, debugText);

	sprintf(debugText, "Model Index %u\n", m_pDynamicEntity->GetModelId().GetModelIndex());
	vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
	grcDebugDraw::Text(vTextRenderPos, colour, debugText);
	
	sprintf(debugText, "Model Name %s\n", CModelInfo::GetBaseModelInfoName(m_pDynamicEntity->GetModelId()));
	vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
	grcDebugDraw::Text(vTextRenderPos, colour, debugText);

	crCreature *creature = m_pDynamicEntity->GetCreature();

	if (creature)
	{
		sprintf(debugText, "Creature Signature %u\n", creature->GetSignature());
		vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
		grcDebugDraw::Text(vTextRenderPos, colour, debugText);


		for (u32 i=0; i<creature->GetNumComponents(); i++)
		{
			crCreatureComponent* pComponent = creature->GetComponentByIndex(i);
			if (pComponent)
			{
				sprintf(debugText, "%s Signature %u\n", pComponent->GetTypeName(), pComponent->GetSignature());
				vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
				grcDebugDraw::Text(vTextRenderPos, colour, debugText);
			}
		}
	}
	else
	{
		sprintf(debugText, "Skeleton Signature %u\n", m_pDynamicEntity->GetSkeletonData().GetSignature());
		vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
		grcDebugDraw::Text(vTextRenderPos, colour, debugText);
	}

	sprintf(debugText, "Number of bones %u\n", m_pDynamicEntity->GetSkeletonData().GetNumBones());
	vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
	grcDebugDraw::Text(vTextRenderPos, colour, debugText);

	sprintf(debugText, "Total number of animatable DOfs %u\n", m_pDynamicEntity->GetSkeletonData().GetNumAnimatableDofs());
	vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
	grcDebugDraw::Text(vTextRenderPos, colour, debugText);

	sprintf(debugText, "Number of translation DOfs %u\n", m_pDynamicEntity->GetSkeletonData().GetNumAnimatableDofs(crBoneData::TRANSLATION));
	vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
	grcDebugDraw::Text(vTextRenderPos, colour, debugText);

	sprintf(debugText, "Number of rotation DOfs %u\n", m_pDynamicEntity->GetSkeletonData().GetNumAnimatableDofs(crBoneData::ROTATION));
	vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
	grcDebugDraw::Text(vTextRenderPos, colour, debugText);

	sprintf(debugText, "Number of scale DOfs %u\n", m_pDynamicEntity->GetSkeletonData().GetNumAnimatableDofs(crBoneData::SCALE));
	vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
	grcDebugDraw::Text(vTextRenderPos, colour, debugText);

	CBaseModelInfo* pBaseModelInfo = m_pDynamicEntity->GetBaseModelInfo();
	animAssert(pBaseModelInfo);
	if (pBaseModelInfo)
	{
		strLocalIndex iExpressionDictionaryIndex = strLocalIndex(pBaseModelInfo->GetExpressionDictionaryIndex());
	
#if __DEV
		char expressionDictionaryName[STORE_NAME_LENGTH];
		if (g_ExpressionDictionaryStore.IsValidSlot(iExpressionDictionaryIndex))
		{
			sprintf(expressionDictionaryName, "Expression dictionary name = %s", g_ExpressionDictionaryStore.GetName(iExpressionDictionaryIndex));
		}
		else
		{
			sprintf(expressionDictionaryName, "Expression dictionary index = %d", iExpressionDictionaryIndex.Get());
		}

		char expressionName[STORE_NAME_LENGTH];
		if (pBaseModelInfo->GetExpressionHash().GetCStr())
		{
			sprintf(expressionName, "Expression name = %s", pBaseModelInfo->GetExpressionHash().GetCStr());
		}
		else
		{
			sprintf(expressionName, "Expression hash = %d", pBaseModelInfo->GetExpressionHash().GetHash());
		}

		sprintf(debugText, "%s\n", expressionDictionaryName);
		vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
		grcDebugDraw::Text(vTextRenderPos, colour, debugText);

		sprintf(debugText, "%s\n", expressionName);
		vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
		grcDebugDraw::Text(vTextRenderPos, colour, debugText);
#else //__DEV
		sprintf(debugText, "Expression dictionary : %d\n", iExpressionDictionaryIndex.Get());
		vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
		grcDebugDraw::Text(vTextRenderPos, colour, debugText);

		sprintf(debugText, "Expression name : %u\n", pBaseModelInfo->GetExpressionHash().GetHash());
		vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
		grcDebugDraw::Text(vTextRenderPos, colour, debugText);
#endif //__DEV

			// Expression set...
		fwMvExpressionSetId exprSetId = pBaseModelInfo->GetExpressionSet();
		if (exprSetId != EXPRESSION_SET_ID_INVALID)
		{
			fwExpressionSet* pExprSet = fwExpressionSetManager::GetExpressionSet(exprSetId);
			if (pExprSet)
			{
				// Expression set name
				char expressionSetName[STORE_NAME_LENGTH];
				if (exprSetId.GetCStr())
				{
					sprintf(expressionSetName, "Expression set name: %s\n", exprSetId.GetCStr());
				}
				else
				{
					sprintf(expressionSetName, "Expression set hash: %d\n", exprSetId.GetHash());
				}

				sprintf(debugText, "%s\n", expressionSetName);
				vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
				grcDebugDraw::Text(vTextRenderPos, colour, debugText);


				// Dictionary
				char expressionSetDict[STORE_NAME_LENGTH];
				if (pExprSet->GetDictionaryName().GetCStr())
				{
					sprintf(expressionSetDict, "--- Expression dictionary name: %s\n", pExprSet->GetDictionaryName().GetCStr());
				}
				else
				{
					sprintf(expressionSetDict, "--- Expression dictionary hash: %d\n", pExprSet->GetDictionaryName().GetHash());
				}

				sprintf(debugText, "%s\n", expressionSetDict);
				vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
				grcDebugDraw::Text(vTextRenderPos, colour, debugText);

				// Expressions
				for (int i=0; i<pExprSet->GetExpressions().GetCount();i++)
				{
					char expressionSetExpr[STORE_NAME_LENGTH];
					if (pExprSet->GetExpressions()[i].GetCStr())
					{
						sprintf(expressionSetExpr, "--- Expression name: %s\n", pExprSet->GetExpressions()[i].GetCStr());
					}
					else
					{
						sprintf(expressionSetExpr, "--- Expression hash: %d\n", pExprSet->GetExpressions()[i].GetHash());
					}

					sprintf(debugText, "%s\n", expressionSetExpr);
					vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
					grcDebugDraw::Text(vTextRenderPos, colour, debugText);
				}
			}
			else
			{
				// Error: Could not be found.
				sprintf(debugText, "Expression Set: Invalid (could not be found)\n");
				vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
				grcDebugDraw::Text(vTextRenderPos, colour, debugText);
			}
		}
		else
		{
			// No expression set
			sprintf(debugText, "Expression Set: null\n");
			vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
			grcDebugDraw::Text(vTextRenderPos, colour, debugText);
		}
	}

	// Facial ClipSet Group...
	/* TODO - FIX ME UP TO USE NEW VARIATIONS DATA
	if (m_pDynamicEntity->GetIsTypePed())
	{
		CPed* pPed = static_cast<CPed*>(m_pDynamicEntity.Get());
		if (pPed)
		{
			CPedModelInfo* pPedModelInfo = pPed->GetPedModelInfo();
			if (pPedModelInfo)
			{
				fwMvFacialClipSetGroupId facialClipSetGroupId = pPedModelInfo->GetFacialClipSetGroupId();
				if (facialClipSetGroupId != FACIAL_CLIP_SET_GROUP_ID_INVALID)
				{
					fwFacialClipSetGroup* pFacialClipSetGroup = fwFacialClipSetGroupManager::GetFacialClipSetGroup(facialClipsetGroupId);
					if (pFacialClipSetGroup)
					{
						// Facial clipset group name
						char groupName[STORE_NAME_LENGTH];
						if (facialClipSetGroupId.GetCStr())
						{
							sprintf(groupName, "Facial clipset group name: %s\n", facialClipSetGroupId.GetCStr());
						}
						else
						{
							sprintf(groupName, "Facial clipset group hash: %d\n", facialClipSetGroupId.GetHash());
						}

						sprintf(debugText, "%s\n", groupName);
						vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
						grcDebugDraw::Text(vTextRenderPos, colour, debugText);

						// Base clipset
						char clipsetName[STORE_NAME_LENGTH];
						if (pFacialClipSetGroup->GetBaseClipSetName().GetCStr())
						{
							sprintf(clipsetName, "--- Base: %s\n", pFacialClipSetGroup->GetBaseClipSetName().GetCStr());
						}
						else
						{
							sprintf(clipsetName, "--- Base: %d\n", pFacialClipSetGroup->GetBaseClipSetName().GetHash());
						}

						sprintf(debugText, "%s\n", clipsetName);
						vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
						grcDebugDraw::Text(vTextRenderPos, colour, debugText);

						// Variations
						}
						else
						{
						// Error: Could not be found.
						sprintf(debugText, "Facial ClipSet Group: Invalid (could not be found)\n");
						vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
						grcDebugDraw::Text(vTextRenderPos, colour, debugText);
					}
				}
				else
				{
					// No facial ClipSet group
					sprintf(debugText, "Facial ClipSet Group: null\n");
					vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
					grcDebugDraw::Text(vTextRenderPos, colour, debugText);
				}
			}
		}
	}*/
}

void CAnimViewer::SetRenderMoverTrackAtFocusTransform()
{
	if (m_pDynamicEntity)
	{
		SetRenderMoverTrackAtTransform(MAT34V_TO_MATRIX34(m_pDynamicEntity->GetMatrix()));
	}
}

void CAnimViewer::SetRenderMoverTrackAtTransform(const Matrix34& mtx)
{
	//set up the mover track rendering to use a fixed transform
	m_renderMoverTrackAtEntity = false;
	m_drawMoverTrack = true;
	m_moverTrackTransform.Set(mtx);
}

void CAnimViewer::SetRenderMoverTrackAtEntity()
{
	//set up the mover track rendering at the entity position
	m_renderMoverTrackAtEntity = true;
	m_drawMoverTrack = true;
	m_moverTrackTransform.Identity();
}

void CAnimViewer::RenderMoverTrack(const crClip* pClip, const Matrix34& mtx)
{	
	if (!pClip)
	{
		animAssertf(pClip, "Cannot render mover track for a null clip.");
		return;
	}

	crFrameDataFixedDofs<4> frameData;
	frameData.AddDof(kTrackBoneTranslation, 0, kFormatTypeVector3);
	frameData.AddDof(kTrackBoneRotation, 0, kFormatTypeQuaternion);  // BUG FIX - was duplicate of TRACK_MOVER_ROTATION in old version
	frameData.AddDof(kTrackMoverTranslation, 0, kFormatTypeVector3);
	frameData.AddDof(kTrackMoverRotation, 0, kFormatTypeQuaternion);

	crFrameFixedDofs<4> frame(frameData);

	pClip->Composite(frame, 0.f);

	Vector3 initialTranslation;
	bool bMoverTrackTranslation = frame.GetValue<Vec3V>(kTrackMoverTranslation, 0, RC_VEC3V(initialTranslation));

	bool bRootBoneTranslation = frame.HasDofValid(kTrackBoneTranslation, 0);

	Quaternion initialOrientation;
	bool bMoverTrackRotation = frame.GetValue<QuatV>(kTrackMoverRotation, 0, RC_QUATV(initialOrientation));
	initialOrientation.Inverse();

	bool bRootBoneRotation = frame.HasDofValid(kTrackBoneRotation, 0);  // BUG FIX - was duplicate of TRACK_MOVER_ROTATION in old version

	TUNE_GROUP_BOOL(CLIP_VIEWER, bMoverTrackSubtractInitialTransform ,true);

	if (bMoverTrackTranslation && bMoverTrackRotation &&  bRootBoneTranslation && bRootBoneRotation)
	{
		// Adding this should advance us one frame
		const int numberOfFrames = int(pClip->GetNum30Frames());

		// Loop through the frames searching for the start and stop loop events
		for (int i=0; i<numberOfFrames; i++)
		{
			pClip->Composite(frame, pClip->Convert30FrameToTime(float(i)));

			Vector3 moverTrackTranslation;
			frame.GetValue<Vec3V>(kTrackMoverTranslation, 0, RC_VEC3V(moverTrackTranslation));
			if(bMoverTrackSubtractInitialTransform)
			{
				moverTrackTranslation -= initialTranslation;
			}

			Vector3 rootBoneTranslation;
			frame.GetValue<Vec3V>(kTrackBoneTranslation, 0, RC_VEC3V(rootBoneTranslation));
			static bool renderRootBoneTrack = false;
			Vector3 translation = moverTrackTranslation;
			if(renderRootBoneTrack)
			{
				translation += rootBoneTranslation;
			}
			translation.Dot(mtx);

			Quaternion moverTrackRotation;
			frame.GetValue<QuatV>(kTrackMoverRotation, 0, RC_QUATV(moverTrackRotation));
			if(bMoverTrackSubtractInitialTransform)
			{
				moverTrackRotation.MultiplyFromLeft(initialOrientation);
			}

			Quaternion rootBoneRotation;
			frame.GetValue<QuatV>(kTrackBoneRotation, 0, RC_QUATV(rootBoneRotation));
			Quaternion rotation;
			rotation.FromMatrix34(mtx);
			rotation.Multiply(moverTrackRotation);
			if(renderRootBoneTrack)
			{
				rotation.Multiply(rootBoneRotation);
			}

			Matrix34 startMat;
			startMat.FromQuaternion(rotation);
			startMat.d = translation;

			grcDebugDraw::Axis(startMat, m_drawMoverTrackSize);
			
			if ((i%10)==0)
			{
				char index[5];
				sprintf(index, "%d", i);
				grcDebugDraw::Text(startMat.d, Color_white, index);
			}
		}
	}
}

void CAnimViewer::DebugRender()
{
	if(!m_pDynamicEntity)
	{
		m_pDynamicEntity = FindPlayerPed();
	}

	if(m_pDynamicEntity && m_pDynamicEntity->GetSkeleton()==NULL)
		return;

	Color32 colour(Color_red);
	char debugText[100];

	int totalBones = m_pDynamicEntity ? m_pDynamicEntity->GetSkeleton()->GetSkeletonData().GetNumBones() : 0;

	// Render Grid - lines on floor at 1.0 meter intervals
	//RenderGrid();

	// Setup to render text
	m_iNoOfTexts = 0;

	Vector2 vTextRenderPos(m_fHorizontalBorder, m_fVerticalBorder + m_iNoOfTexts*m_fVertDist);

	//////////////////////////////////////////////////////////////////////////
	//
	//	Visualise the motion tree for the selected entity
	//
	//////////////////////////////////////////////////////////////////////////
	if(m_bCaptureMotionTree)
	{
		if(fwTimer::GetSingleStepThisFrame() || (!fwTimer::IsUserPaused() && !fwTimer::IsScriptPaused() && !fwTimer::IsGamePaused()))
		{
			if(!m_bCapturingMotionTree)
			{
				// Start capturing
				m_MoveDump->Reset();

				m_bCapturingMotionTree = true;
			}

			// Capturing
			const char *szFilter = NULL;
			if(m_iCaptureFilterNameIndex >= 0 && m_iCaptureFilterNameIndex < m_iCaptureFilterNameCount)
			{
				szFilter = m_szCaptureFilterNames[m_iCaptureFilterNameIndex];
			}
			else
			{
				szFilter = "BONEMASK_LOD_LO";
			}
			m_MoveDump->Capture(m_pDynamicEntity, m_bCaptureDofs, szFilter, m_bRelativeToParent);

		}
	}
	else
	{
		if(m_bCapturingMotionTree)
		{
			// Stop capturing
			m_bCapturingMotionTree = false;
		}
	}

	EntityCaptureRenderUpdate();

	if(m_bCameraCapture_Started)
	{
		if(fwTimer::GetSingleStepThisFrame() || (!fwTimer::IsUserPaused() && !fwTimer::IsScriptPaused() && !fwTimer::IsGamePaused()))
		{
			CameraCapture_Update();
		}
	}

	if (m_bVisualiseMotionTree)
	{
		VisualiseMotionTree();
	}

	if (m_bVisualiseMotionTreeSimple || m_bDumpMotionTreeSimple)
	{
		VisualiseMotionTreeSimple();
		m_bDumpMotionTreeSimple = false;
	}

	if (!m_active)
		return;

#if __DEV
	if (m_renderPointCloud || m_renderLastMatchedPose)
	{
		RenderPointCloud();
	}	
#endif //__DEV	

	if (!ms_DofTrackerUpdatePostPhysics)
	{
		UpdateDOFTrackingForEKG();
	}

	///////////////////////////////////////////////////////////////////////////////
	//
	// Render a matrix for every frame of the mover track or root bone for the selected animation
	//
	///////////////////////////////////////////////////////////////////////////////
	if (m_animSelector.GetSelectedClip() && m_drawMoverTrack)
	{
		const crClip* pClip = m_animSelector.GetSelectedClip();
		

		if (m_renderMoverTrackAtEntity)
		{
			Matrix34 m = MAT34V_TO_MATRIX34(m_pDynamicEntity->GetMatrix());
			RenderMoverTrack(pClip, m);
		}
		else
		{
			RenderMoverTrack(pClip, m_moverTrackTransform);
		}
		
	}

	///////////////////////////////////////////////////////////////////////////////
	//
	// Draw the world matrix (at the characters position)
	//
	///////////////////////////////////////////////////////////////////////////////
	static bool drawWorldMatrix = false;
	if (drawWorldMatrix)
	{
		Matrix34 mat;
		mat.Identity();
		mat.d = VEC3V_TO_VECTOR3(m_pDynamicEntity->GetTransform().GetPosition());
		grcDebugDraw::Axis(mat, 1.0f, true);
	}

	///////////////////////////////////////////////////////////////////////////////
	//
	//  Characters matrix information
	//
	///////////////////////////////////////////////////////////////////////////////
	if (m_drawSelectedEntityMat)
	{
		// Render the character matrix
		Matrix34 m = MAT34V_TO_MATRIX34(m_pDynamicEntity->GetMatrix());
		grcDebugDraw::Axis(m, m_drawSelectedEntityMatSize, true);

		// Output the selected entities global translation
		if (m_drawSelectedEntityMatTranslation)
		{
			Vector3 localTranslation = VEC3V_TO_VECTOR3(m_pDynamicEntity->GetTransform().GetPosition());

			sprintf(debugText,"Translation  %7.4f, %7.4f, %7.4f \n", localTranslation.x, localTranslation.y, localTranslation.z);		
			vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
			grcDebugDraw::Text(vTextRenderPos, colour, debugText);
		}

		// Output the selected entities global rotation
		if (m_drawSelectedEntityMatRotation)
		{
			Matrix34 m = MAT34V_TO_MATRIX34(m_pDynamicEntity->GetMatrix());
			Vector3 localEuler = m.GetEulers();

			if (m_eulers)
			{
				// Output the selected bones local and global rotation (as degrees/eulers)
				if (m_degrees)
				{
					sprintf(debugText,"Rotation %7.4f, %7.4f, %7.4f \n", localEuler.x*RtoD, localEuler.y*RtoD, localEuler.z*RtoD);
				
					vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
					grcDebugDraw::Text(vTextRenderPos, colour, debugText);
				}
				else
				{
					sprintf(debugText,"Rotation %7.4f, %7.4f, %7.4f \n", localEuler.x, localEuler.y, localEuler.z);
				
					vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
					grcDebugDraw::Text(vTextRenderPos, colour, debugText);
				}
			}
			else
			{
				// Output the selected bones local and global matrix (as matrices)
				const Matrix34 localMat = MAT34V_TO_MATRIX34(m_pDynamicEntity->GetMatrix());

				sprintf(debugText, "Local Rotation \n");
				vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
				grcDebugDraw::Text(vTextRenderPos, colour, debugText);

				sprintf(debugText, "%10.6f, %10.6f, %10.6f \n", localMat.a.x, localMat.a.y, localMat.a.z);			
				vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
				grcDebugDraw::Text(vTextRenderPos, colour, debugText);

				sprintf(debugText, "%10.6f, %10.6f, %10.6f \n", localMat.b.x, localMat.b.y, localMat.b.z);			
				vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
				grcDebugDraw::Text(vTextRenderPos, colour, debugText);

				sprintf(debugText, "%10.6f, %10.6f, %10.6f \n", localMat.c.x, localMat.c.y, localMat.c.z);			
				vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
				grcDebugDraw::Text(vTextRenderPos, colour, debugText);
			}
		}

		if (m_drawSelectedEntityMatTransAndRotWrtCutscene && CutSceneManager::GetInstance()->IsCutscenePlayingBack())
		{
			Matrix34 cutsceneAnimMat;
			CutSceneManager::GetInstance()->GetSceneOrientationMatrix(cutsceneAnimMat);
			Vector3 cutsceneEuler = cutsceneAnimMat.GetEulers();

			sprintf(debugText,"Cutscene Rotation %7.4f, %7.4f, %7.4f \n", cutsceneEuler.x*RtoD, cutsceneEuler.y*RtoD, cutsceneEuler.z*RtoD);
			vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
			grcDebugDraw::Text(vTextRenderPos, colour, debugText);

			sprintf(debugText,"Cutscene Translation %7.4f, %7.4f, %7.4f \n", cutsceneAnimMat.d.x, cutsceneAnimMat.d.y, cutsceneAnimMat.d.z);
			vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
			grcDebugDraw::Text(vTextRenderPos, colour, debugText);

			Matrix34 globalMat = MAT34V_TO_MATRIX34(m_pDynamicEntity->GetMatrix());
			Matrix34 cutsceneRelativeAnimMat(globalMat);
			cutsceneRelativeAnimMat.DotTranspose(cutsceneAnimMat);
			Vector3 cutsceneRelativeEuler = cutsceneRelativeAnimMat.GetEulers();

			sprintf(debugText,"Cutscene relative Rotation %7.4f, %7.4f, %7.4f \n", cutsceneRelativeEuler.x*RtoD, cutsceneRelativeEuler.y*RtoD, cutsceneRelativeEuler.z*RtoD);
			vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
			grcDebugDraw::Text(vTextRenderPos, colour, debugText);

			sprintf(debugText,"Cutscene relative Translation %7.4f, %7.4f, %7.4f \n", cutsceneRelativeAnimMat.d.x, cutsceneRelativeAnimMat.d.y, cutsceneRelativeAnimMat.d.z);
			vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
			grcDebugDraw::Text(vTextRenderPos, colour, debugText);
		}
	}

	///////////////////////////////////////////////////////////////////////////////
	//
	//  Motion blur info
	//
	///////////////////////////////////////////////////////////////////////////////

	if (m_drawMotionBlurData)
	{
		const CClipEventTags::CMotionBlurEventTag::MotionBlurData* pData = CClipEventTags::GetCurrentMotionBlur();

		if (pData)
		{
			sprintf(debugText,"Motion blur - blend: %.3f, Current Pos: %.3f %.3f %.3f, amount: %.3f\n", pData->GetBlend(), pData->GetCurrentPosition().GetXf(), pData->GetCurrentPosition().GetYf(), pData->GetCurrentPosition().GetZf(), pData->GetCurrentBlurAmount());
			vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
			grcDebugDraw::Text(vTextRenderPos, colour, debugText);
		}
		else
		{
			sprintf(debugText,"No motion blur active\n");
			vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
			grcDebugDraw::Text(vTextRenderPos, colour, debugText);
		}
	}

	///////////////////////////////////////////////////////////////////////////////
	//
	// Render the tags for the selected animation.
	//
	///////////////////////////////////////////////////////////////////////////////
	if (m_renderTagsForSelectedClip && m_animSelector.GetSelectedClip())
	{
		const crClip* pClip = m_animSelector.GetSelectedClip();

		const crTags* pTags = pClip->GetTags();
		if (pTags)
		{
			// Clip text
			sprintf(debugText, "Tags for clip %s...\n", pClip->GetName());
			vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
			grcDebugDraw::Text(vTextRenderPos, colour, debugText);

			for (int i=0; i < pTags->GetNumTags(); i++)
			{
				const crTag* pTag = pTags->GetTag(i);
				if (pTag)
				{
					const float fStartTime = pTag->GetStart();
					const float fEndTime = pTag->GetEnd();

					static CClipEventTags::Key moveEventKey("MoveEvent");
					static CClipEventTags::Key visibleToScriptKey("VisibleToScript");
					static CClipEventTags::Key audioKey("Audio");
					static CClipEventTags::Key blockKey("Block");
					bool bNeedsGenericTagText = true;

					if (pTag->GetKey() == moveEventKey)
					{
						const crPropertyAttribute* pAttrib = pTag->GetProperty().GetAttribute("MoveEvent");
						if (pAttrib && pAttrib->GetType() == crPropertyAttribute::kTypeHashString)
						{
							const crPropertyAttributeHashString* pTagHashString = static_cast<const crPropertyAttributeHashString*>(pAttrib);
							if (pTagHashString->GetHashString().TryGetCStr())
							{
								sprintf(debugText, "Tag: MoveEvent (%s) T(%.4f - %.4f), F(%.4f - %.4f), P(%.6f - %.6f)\n", pTagHashString->GetHashString().TryGetCStr(), 
									fStartTime, fEndTime, 
									pClip->ConvertTimeTo30Frame(fStartTime),
									pClip->ConvertTimeTo30Frame(fEndTime),
									pClip->ConvertTimeToPhase(fStartTime),
									pClip->ConvertTimeToPhase(fEndTime));
							}
							else
							{
								sprintf(debugText, "Tag: MoveEvent (%u) T(%.4f - %.4f), F(%.4f - %.4f), P(%.6f - %.6f)\n", pTagHashString->GetHashString().GetHash(), 
									fStartTime, fEndTime, 
									pClip->ConvertTimeTo30Frame(fStartTime),
									pClip->ConvertTimeTo30Frame(fEndTime),
									pClip->ConvertTimeToPhase(fStartTime),
									pClip->ConvertTimeToPhase(fEndTime));
							}

							bNeedsGenericTagText = false;
						}
					}
					else if (pTag->GetKey() == visibleToScriptKey)
					{
						const crPropertyAttribute* pAttrib = pTag->GetProperty().GetAttribute("Event");
						if (pAttrib && pAttrib->GetType() == crPropertyAttribute::kTypeHashString)
						{
							const crPropertyAttributeHashString* pTagHashString = static_cast<const crPropertyAttributeHashString*>(pAttrib);
							if (pTagHashString->GetHashString().TryGetCStr())
							{
								sprintf(debugText, "Tag: VisibleToScript (%s) (%.2f - %.2f)\n", pTagHashString->GetHashString().TryGetCStr(), fStartTime, fEndTime);
							}
							else
							{
								sprintf(debugText, "Tag: VisibleToScript (%u) (%.2f - %.2f)\n", pTagHashString->GetHashString().GetHash(), fStartTime, fEndTime);
							}

							bNeedsGenericTagText = false;
						}
					}
					else if (pTag->GetKey() == audioKey)
					{
						const crPropertyAttribute* pAttrib = pTag->GetProperty().GetAttribute("Id");
						if (pAttrib && pAttrib->GetType() == crPropertyAttribute::kTypeHashString)
						{
							const crPropertyAttributeHashString* pTagHashString = static_cast<const crPropertyAttributeHashString*>(pAttrib);
							if (pTagHashString->GetHashString().TryGetCStr())
							{
								sprintf(debugText, "Tag: Audio (%s) (%.2f - %.2f)\n", pTagHashString->GetHashString().TryGetCStr(), fStartTime, fEndTime);
							}
							else
							{
								sprintf(debugText, "Tag: Audio (%u) (%.2f - %.2f)\n", pTagHashString->GetHashString().GetHash(), fStartTime, fEndTime);
							}

							bNeedsGenericTagText = false;
						}
					}
					
					if (bNeedsGenericTagText)
					{
						// Generic text
						if (atHashString(pTag->GetProperty().GetKey()).TryGetCStr())
						{
							sprintf(debugText, "Tag: MoveEvent (%s) T(%.4f - %.4f), F(%.4f - %.4f), P(%.4f - %.4f)\n", 
								atHashString(pTag->GetProperty().GetKey()).TryGetCStr(), 
								fStartTime, fEndTime, 
								pClip->ConvertTimeTo30Frame(fStartTime),
								pClip->ConvertTimeTo30Frame(fEndTime),
								pClip->ConvertTimeToPhase(fStartTime),
								pClip->ConvertTimeToPhase(fEndTime));

						}
						else
						{
							sprintf(debugText, "Tag: MoveEvent (%u) T(%.4f - %.4f), F(%.4f - %.4f), P(%.4f - %.4f)\n", 
								pTag->GetProperty().GetKey(), 
								fStartTime, fEndTime, 
								pClip->ConvertTimeTo30Frame(fStartTime),
								pClip->ConvertTimeTo30Frame(fEndTime),
								pClip->ConvertTimeToPhase(fStartTime),
								pClip->ConvertTimeToPhase(fEndTime));
						}
					}
					
					vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
					grcDebugDraw::Text(vTextRenderPos, colour, debugText);

					// Render the tags visually?
					if (m_renderTagsVisuallyForSelectedClip)
					{
						const float fClipStart = m_renderTagsVisuallyForSelectedClipOffset;
						const float fClipLineLength = 0.3f;

						// Draw Clip Lines
						Vector2 vPos1(fClipStart, vTextRenderPos.y);
						Vector2 vPos2(fClipStart, vTextRenderPos.y + 0.011f);
						grcDebugDraw::Line(vPos1, vPos2, Color_white);

						vPos1.x = fClipStart + fClipLineLength;
						vPos2.x = fClipStart + fClipLineLength;
						grcDebugDraw::Line(vPos1, vPos2, Color_white);

						vPos1.x = fClipStart;
						vPos1.y = vTextRenderPos.y + 0.0055f;
						vPos2.x = fClipStart + fClipLineLength;
						vPos2.y = vTextRenderPos.y + 0.0055f;
						grcDebugDraw::Line(vPos1, vPos2, Color_white);

						// Draw Tag
						vPos1.Set(fClipStart + (fStartTime * fClipLineLength), vTextRenderPos.y);
						vPos2.Set(fClipStart + (fStartTime * fClipLineLength), vTextRenderPos.y + 0.011f);
						grcDebugDraw::Line(vPos1, vPos2, Color_red);

						// Draw duration?
						if (fEndTime > fStartTime)
						{
							// End
							vPos1.Set(fClipStart + (fEndTime * fClipLineLength), vTextRenderPos.y);
							vPos2.Set(fClipStart + (fEndTime * fClipLineLength), vTextRenderPos.y + 0.011f);
							grcDebugDraw::Line(vPos1, vPos2, Color_red);

							// Middle duration
							vPos1.Set(fClipStart + (fEndTime * fClipLineLength), vTextRenderPos.y + 0.0055f);
							vPos2.Set(fClipStart + (fStartTime * fClipLineLength), vTextRenderPos.y + 0.0055f);
							grcDebugDraw::Line(vPos1, vPos2, Color_red);
						}
					}
				}				
			}
		}
	}

	///////////////////////////////////////////////////////////////////////////////
	//
	// Selected bone information
	//
	if (m_drawSelectedBoneMat)
	{
		// Get the selected bones local and global matrices
		const crBoneData *boneData = m_pDynamicEntity->GetSkeleton()->GetSkeletonData().GetBoneData(m_boneIndex);
		const Matrix34 &localMat = m_pDynamicEntity->GetLocalMtx(m_boneIndex);
		const Matrix34 &objectMat = m_pDynamicEntity->GetObjectMtx(m_boneIndex);
		
		Matrix34 globalMat;
		m_pDynamicEntity->GetGlobalMtx(m_boneIndex, globalMat);

		// Render the selected bones matrix
		grcDebugDraw::Axis(globalMat, m_drawSelectedBoneMatSize, true);

		if(m_drawVehicleComponentBoundingBox && m_pDynamicEntity->GetType()==ENTITY_TYPE_VEHICLE)
		{
			CVehicle * pVehicle = (CVehicle*)m_pDynamicEntity.Get();
			phInst * pInst = m_pDynamicEntity->GetFragInst() ? m_pDynamicEntity->GetFragInst() : m_pDynamicEntity->GetCurrentPhysicsInst();
			const int iComponent = pVehicle->GetFragInst()->GetType()->GetComponentFromBoneIndex(((fragInst*)pInst)->GetCurrentPhysicsLOD(), m_boneIndex);

			if(iComponent != -1)
			{
				phBound* pBound = pInst->GetArchetype()->GetBound();

				Matrix34 currMat = RCC_MATRIX34(pInst->GetMatrix());	// CLIP_NEW_VEC_LIB
				if(pBound->GetType() == phBound::COMPOSITE)
				{
					phBoundComposite* pBoundComposite = (phBoundComposite*)pBound;
					animAssert(iComponent < pBoundComposite->GetNumBounds());
					phBound* pChildBound = pBoundComposite->GetBound(iComponent);
					if (pChildBound)
					{
						// calc the new matrix
						const Matrix34 & thisMat = RCC_MATRIX34(pBoundComposite->GetCurrentMatrix(iComponent));
						currMat.DotFromLeft(thisMat);
						if(fwTimer::GetSystemFrameCount()&1)
						{
							const Vector3 & vMin = VEC3V_TO_VECTOR3(pChildBound->GetBoundingBoxMin());
							const Vector3 & vMax = VEC3V_TO_VECTOR3(pChildBound->GetBoundingBoxMax());
							CDebugScene::Draw3DBoundingBox(vMin, vMax, currMat, Color_white);
						}
					}
				}
			}
		}

		///////////////////////////////////////////////////////////////////////////////
		//
		// Render the selected bones joint limits
		//
		///////////////////////////////////////////////////////////////////////////////

		///////////////////////////////////////////////////////////////////////////////
		//
		// Output the selected bones name, index, id and whether it supports rotation and translation
		//
		///////////////////////////////////////////////////////////////////////////////
		sprintf(debugText,"%s\n", boneData->GetName());
	
		vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
		grcDebugDraw::Text(vTextRenderPos, colour, debugText);

		sprintf(debugText,"ID = %d\n", boneData->GetBoneId());
	
		vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
		grcDebugDraw::Text(vTextRenderPos, colour, debugText);

		sprintf(debugText,"Index = %d\n", boneData->GetIndex());
	
		vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
		grcDebugDraw::Text(vTextRenderPos, colour, debugText);

		bool bX, bY, bZ;

		const bool hasRotationDofs = boneData->HasDofs(crBoneData::ROTATION);
		bX = false;
		bY = false;
		bZ = false;

		if (boneData->HasDofs(crBoneData::ROTATE_X)) { bX = true; }
		if (boneData->HasDofs(crBoneData::ROTATE_Y)) { bY = true; }
		if (boneData->HasDofs(crBoneData::ROTATE_Z)) { bZ = true; }

		sprintf(debugText, "Rotation Dofs:\n");
		vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
		grcDebugDraw::Text(vTextRenderPos, colour, debugText);

		if (hasRotationDofs)
		{
			sprintf(debugText, "%s %s %s\n", bX ? "X" : "", bY ? "Y" : "", bZ ? "Z" : "");
			vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
			grcDebugDraw::Text(vTextRenderPos, colour, debugText);
		}
	
		const bool hasTranslationDofs = boneData->HasDofs(crBoneData::TRANSLATION);
		bX = false;
		bY = false;
		bZ = false;

		if (boneData->HasDofs(crBoneData::TRANSLATE_X)) { bX = true; }
		if (boneData->HasDofs(crBoneData::TRANSLATE_Y)) { bY = true; }
		if (boneData->HasDofs(crBoneData::TRANSLATE_Z)) { bZ = true; }
	
		sprintf(debugText, "Translation Dofs:\n");
		vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
		grcDebugDraw::Text(vTextRenderPos, colour, debugText);

		if (hasTranslationDofs)
		{
			sprintf(debugText, "%s %s %s\n", bX ? "X" : "", bY ? "Y" : "", bZ ? "Z" : "");
			vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
			grcDebugDraw::Text(vTextRenderPos, colour, debugText);
		}
	
		const bool hasScaleDofs = boneData->HasDofs(crBoneData::SCALE);
		bX = false;
		bY = false;
		bZ = false;

		if (boneData->HasDofs(crBoneData::SCALE_X)) { bX = true; }
		if (boneData->HasDofs(crBoneData::SCALE_Y)) { bY = true; }
		if (boneData->HasDofs(crBoneData::SCALE_Z)) { bZ = true; }
	
		sprintf(debugText, "Scale Dofs:\n");
		vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
		grcDebugDraw::Text(vTextRenderPos, colour, debugText);

		if (hasScaleDofs)
		{
			sprintf(debugText, "%s %s %s\n", bX ? "X" : "", bY ? "Y" : "", bZ ? "Z" : "");
			vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
			grcDebugDraw::Text(vTextRenderPos, colour, debugText);
		}

		// Output the selected bones local and global translation
		if (m_drawSelectedBoneMatTranslation)
		{
			///////////////////////////////////////////////////////////////////////////////
			//
			// Output the selected bones local and global translation (as eulers)
			//
			Vector3 localTranslation = localMat.d;
			Vector3 objectTranslation = objectMat.d;
			Vector3 globalTranslation = globalMat.d;

			sprintf(debugText,"Local Translation %7.4f, %7.4f, %7.4f \n", localTranslation.x, localTranslation.y, localTranslation.z);
		
			vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
			grcDebugDraw::Text(vTextRenderPos, colour, debugText);

			sprintf(debugText,"Object Translation %7.4f, %7.4f, %7.4f \n", objectTranslation.x, objectTranslation.y, objectTranslation.z);

			vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
			grcDebugDraw::Text(vTextRenderPos, colour, debugText);

			sprintf(debugText,"Global Translation %7.4f, %7.4f, %7.4f \n", globalTranslation.x, globalTranslation.y, globalTranslation.z);
		
			vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
			grcDebugDraw::Text(vTextRenderPos, colour, debugText);
			///////////////////////////////////////////////////////////////////////////////
		}

		// Output the selected bones local and global rotation
		if (m_drawSelectedBoneMatRotation)
		{
			///////////////////////////////////////////////////////////////////////////////
			//
			// Output the selected bones local and global matrix (as eulers)
			//
			if (m_eulers)
			{
				Vector3 localEuler = localMat.GetEulers();
				Vector3 objectEuler = objectMat.GetEulers();
				Vector3 globalEuler = globalMat.GetEulers();
				if (m_degrees)
				{
					localEuler *= RtoD;
					objectEuler *= RtoD;
					globalEuler *= RtoD;

					if (m_drawSelectedBoneMatTransAndRotWrtCutscene && CutSceneManager::GetInstance()->IsCutscenePlayingBack())
					{
						Matrix34 cutsceneAnimMat;
						CutSceneManager::GetInstance()->GetSceneOrientationMatrix(cutsceneAnimMat);
						//Vector3 cutsceneEuler = cutsceneAnimMat.GetEulers();

						//sprintf(debugText,"Cutscene Rotation %7.4f, %7.4f, %7.4f \n", cutsceneEuler.x*RtoD, cutsceneEuler.y*RtoD, cutsceneEuler.z*RtoD);
						//vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
						//grcDebugDraw::Text(vTextRenderPos, colour, debugText);

						//sprintf(debugText,"Cutscene Translation %7.4f, %7.4f, %7.4f \n", cutsceneAnimMat.d.x, cutsceneAnimMat.d.y, cutsceneAnimMat.d.z);
						//vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
						//grcDebugDraw::Text(vTextRenderPos, colour, debugText);

						Matrix34 cutsceneRelativeAnimMat(globalMat);
						cutsceneRelativeAnimMat.DotTranspose(cutsceneAnimMat);
						Vector3 cutsceneRelativeEuler = cutsceneRelativeAnimMat.GetEulers();

						sprintf(debugText,"Cutscene relative Rotation %7.4f, %7.4f, %7.4f \n", cutsceneRelativeEuler.x*RtoD, cutsceneRelativeEuler.y*RtoD, cutsceneRelativeEuler.z*RtoD);
						vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
						grcDebugDraw::Text(vTextRenderPos, colour, debugText);

						sprintf(debugText,"Cutscene relative Translation %7.4f, %7.4f, %7.4f \n", cutsceneRelativeAnimMat.d.x, cutsceneRelativeAnimMat.d.y, cutsceneRelativeAnimMat.d.z);
						vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
						grcDebugDraw::Text(vTextRenderPos, colour, debugText);
					}
				}
				sprintf(debugText,"Local Rotation %7.4f, %7.4f, %7.4f \n", localEuler.x, localEuler.y, localEuler.z);

				vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
				grcDebugDraw::Text(vTextRenderPos, colour, debugText);

				sprintf(debugText,"Object Rotation %7.4f, %7.4f, %7.4f \n", objectEuler.x, objectEuler.y, objectEuler.z);

				vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
				grcDebugDraw::Text(vTextRenderPos, colour, debugText);

				sprintf(debugText,"Global Rotation %7.4f, %7.4f, %7.4f \n", globalEuler.x, globalEuler.y, globalEuler.z);

				vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
				grcDebugDraw::Text(vTextRenderPos, colour, debugText);
			}
			else
			{
				///////////////////////////////////////////////////////////////////////////////
				//
				// Output the selected bones local and global matrix (as matrices)
				//
				sprintf(debugText, "Local Rotation \n");
			
				vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
				grcDebugDraw::Text(vTextRenderPos, colour, debugText);

				sprintf(debugText, "%10.6f, %10.6f, %10.6f \n", localMat.a.x, localMat.a.y, localMat.a.z);

				vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
				grcDebugDraw::Text(vTextRenderPos, colour, debugText);

				sprintf(debugText, "%10.6f, %10.6f, %10.6f \n", localMat.b.x, localMat.b.y, localMat.b.z);

				vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
				grcDebugDraw::Text(vTextRenderPos, colour, debugText);

				sprintf(debugText, "%10.6f, %10.6f, %10.6f \n", localMat.c.x, localMat.c.y, localMat.c.z);

				vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
				grcDebugDraw::Text(vTextRenderPos, colour, debugText);

				sprintf(debugText, "Object Rotation \n");

				vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
				grcDebugDraw::Text(vTextRenderPos, colour, debugText);

				sprintf(debugText, "%10.6f, %10.6f, %10.6f \n", objectMat.a.x, objectMat.a.y, objectMat.a.z);

				vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
				grcDebugDraw::Text(vTextRenderPos, colour, debugText);

				sprintf(debugText, "%10.6f, %10.6f, %10.6f \n", objectMat.b.x, objectMat.b.y, objectMat.b.z);

				vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
				grcDebugDraw::Text(vTextRenderPos, colour, debugText);

				sprintf(debugText, "%10.6f, %10.6f, %10.6f \n", objectMat.c.x, objectMat.c.y, objectMat.c.z);

				vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
				grcDebugDraw::Text(vTextRenderPos, colour, debugText);

				sprintf(debugText, "Global Rotation \n");

				vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
				grcDebugDraw::Text(vTextRenderPos, colour, debugText);

				sprintf(debugText, "%10.6f, %10.6f, %10.6f \n", globalMat.a.x, globalMat.a.y, globalMat.a.z);

				vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
				grcDebugDraw::Text(vTextRenderPos, colour, debugText);

				sprintf(debugText, "%10.6f, %10.6f, %10.6f \n", globalMat.b.x, globalMat.b.y, globalMat.b.z);

				vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
				grcDebugDraw::Text(vTextRenderPos, colour, debugText);

				sprintf(debugText, "%10.6f, %10.6f, %10.6f \n", globalMat.c.x, globalMat.c.y, globalMat.c.z);
			
				vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
				grcDebugDraw::Text(vTextRenderPos, colour, debugText);
				///////////////////////////////////////////////////////////////////////////////

			}
			///////////////////////////////////////////////////////////////////////////////
		}



		// Output the selected bones local and global scale
		if (m_drawSelectedBoneMatScale)
		{
			///////////////////////////////////////////////////////////////////////////////
			//
			// Output the selected bones local and global scale
			//
			Vec3V localScale = ScaleFromMat33VTranspose(RCC_MAT34V(localMat).GetMat33());
			Vec3V objectScale = ScaleFromMat33VTranspose(RCC_MAT34V(objectMat).GetMat33());
			Vec3V globalScale = ScaleFromMat33VTranspose(RCC_MAT34V(globalMat).GetMat33());

			sprintf(debugText,"Local Scale %7.4f, %7.4f, %7.4f \n", localScale.GetXf(), localScale.GetYf(), localScale.GetZf());

			vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
			grcDebugDraw::Text(vTextRenderPos, colour, debugText);

			sprintf(debugText,"Object Scale %7.4f, %7.4f, %7.4f \n", objectScale.GetXf(), objectScale.GetYf(), objectScale.GetZf());

			vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
			grcDebugDraw::Text(vTextRenderPos, colour, debugText);

			sprintf(debugText,"Global Scale %7.4f, %7.4f, %7.4f \n", globalScale.GetXf(), globalScale.GetYf(), globalScale.GetZf());

			vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
			grcDebugDraw::Text(vTextRenderPos, colour, debugText);
			///////////////////////////////////////////////////////////////////////////////
		}

		sprintf(debugText, "\n");
		
		vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
		grcDebugDraw::Text(vTextRenderPos, colour, debugText);
	}
	///////////////////////////////////////////////////////////////////////////////
	if (m_drawSelectedParentBoneMat)
	{
		int parentBoneIndex = m_pDynamicEntity->GetSkeletonData().GetParentIndex(m_boneIndex);
		if(parentBoneIndex >= 0 && parentBoneIndex < m_pDynamicEntity->GetSkeletonData().GetNumBones())
		{
			// Get the selected bones local and global matrices
			//const Matrix34 &localMat = m_pDynamicEntity->GetLocalMtx(parentBoneIndex);

			Matrix34 globalMat;
			m_pDynamicEntity->GetGlobalMtx(parentBoneIndex, globalMat);

			// Render the selected bones matrix
			grcDebugDraw::Axis(globalMat, m_drawSelectedBoneMatSize, true);
		}
	}

	///////////////////////////////////////////////////////////////////////////////
	//
	// Draw all bones matrices ?
	//
	if (m_drawAllBonesMat)
	{
		for (int i = 1; i < totalBones; i++)
		{
			if (m_drawAllBonesMat)
			{
				if (m_drawSelectedBoneMat && i == m_boneIndex )
				{
				}
				else
				{
					
					Matrix34 globalMat;
					m_pDynamicEntity->GetGlobalMtx(i, globalMat);

					grcDebugDraw::Axis(globalMat, m_drawAllBonesMatSize, true);
				}
			}
		}
	}
	///////////////////////////////////////////////////////////////////////////////

	///////////////////////////////////////////////////////////////////////////////
	//
	// Render the debug skeleton array
	//
	///////////////////////////////////////////////////////////////////////////////

	if (m_drawDebugSkeletonArray)
	{
		for(s32 j=0; j<m_debugSkeletons.GetCount(); j++)
		{
			if (m_debugSkeletons[j].m_bRender)
			{
				const crSkeleton &skeleton = m_debugSkeletons[j].m_skeleton;

				if (m_drawDebugSkeletonsUsingLocals)
				{
					RenderSkeletonUsingLocals(skeleton, m_debugSkeletons[j].m_color);
				}
				else
				{
					RenderSkeleton(skeleton, m_debugSkeletons[j].m_color);
				}
				
				if (m_renderDebugSkeletonParentMatrices)
				{
					grcDebugDraw::Axis(m_debugSkeletons[j].m_parentMat, m_drawSelectedEntityMatSize, true);
				}				

				if (m_debugSkeletons[j].m_description!=NULL && m_renderDebugSkeletonNames)
				{
					grcDebugDraw::Text(RCC_MATRIX34(*skeleton.GetParentMtx()).d,m_debugSkeletons[j].m_color,0,0,&m_debugSkeletons[j].m_description[0],true);
				}
			}
		}
	}

	///////////////////////////////////////////////////////////////////////////////
	//
	// Render the pose matched blend out
	//
	///////////////////////////////////////////////////////////////////////////////

	for (atSNode<BlendOutData>* blendOutData = m_BlendOutData.GetHead(); blendOutData != NULL; blendOutData = blendOutData->GetNext())
	{
		if (animVerifyf(blendOutData->Data.m_SkeletonData != NULL, "CAnimViewer::DebugRender: Invalid skeleton data"))
		{
			crSkeleton skeleton;
			if (blendOutData->Data.m_DrawSelectedBlendOutSkeleton)
			{
				if (animVerifyf(blendOutData->Data.m_SelectedFrame != NULL, "CAnimViewer::DebugRender: Invalid selected frame") && blendOutData->Data.m_SelectedFrame->HasDofValid(kTrackBoneTranslation, 0))
				{
					if (skeleton.GetParentMtx() == NULL)
					{
						skeleton.Init(*blendOutData->Data.m_SkeletonData, &blendOutData->Data.m_SelectedTransform);
					}
					else
					{
						skeleton.SetParentMtx(&blendOutData->Data.m_SelectedTransform);
					}
					blendOutData->Data.m_SelectedFrame->Pose(skeleton, false, blendOutData->Data.m_UseLowLODFilter ? blendOutData->Data.m_LowLODFrameFilter : NULL);
					skeleton.Update();
					RenderSkeleton(skeleton, Color_red, blendOutData->Data.m_UseLowLODFilter ? blendOutData->Data.m_LowLODFrameFilter : NULL);
				}
			}

			if (blendOutData->Data.m_DrawMatchedBlendOutSkeleton)
			{
				if (animVerifyf(blendOutData->Data.m_MatchedFrame != NULL, "CAnimViewer::DebugRender: Invalid matched frame") && blendOutData->Data.m_MatchedFrame->HasDofValid(kTrackBoneTranslation, 0))
				{
					if (skeleton.GetParentMtx() == NULL)
					{
						skeleton.Init(*blendOutData->Data.m_SkeletonData, &blendOutData->Data.m_MatchedTransform);
					}
					else
					{
						skeleton.SetParentMtx(&blendOutData->Data.m_MatchedTransform);
					}
					blendOutData->Data.m_MatchedFrame->Pose(skeleton, false, blendOutData->Data.m_UseLowLODFilter ? blendOutData->Data.m_LowLODFrameFilter : NULL);
					skeleton.Update();
					RenderSkeleton(skeleton, Color_green, blendOutData->Data.m_UseLowLODFilter ? blendOutData->Data.m_LowLODFrameFilter : NULL);
				}
			}

			if (blendOutData->Data.m_DrawRagdollFrameSkeleton)
			{
				if (animVerifyf(blendOutData->Data.m_RagdollFrame != NULL, "CAnimViewer::DebugRender: Invalid ragdoll frame") && blendOutData->Data.m_RagdollFrame->HasDofValid(kTrackBoneTranslation, 0))
				{
					if (skeleton.GetParentMtx() == NULL)
					{
						skeleton.Init(*blendOutData->Data.m_SkeletonData, &blendOutData->Data.m_RagdollTransform);
					}
					else
					{
						skeleton.SetParentMtx(&blendOutData->Data.m_RagdollTransform);
					}
					blendOutData->Data.m_RagdollFrame->Pose(skeleton, false, blendOutData->Data.m_UseLowLODFilter ? blendOutData->Data.m_LowLODFrameFilter : NULL);
					skeleton.Update();
					RenderSkeleton(skeleton, Color_yellow, blendOutData->Data.m_UseLowLODFilter ? blendOutData->Data.m_LowLODFrameFilter : NULL);
				}
			}
		}
	}

	///////////////////////////////////////////////////////////////////////////////
	//
	//	Render the skeleton for the selected animation
	//	TODO - make this available through the motion tree visualisation
	///////////////////////////////////////////////////////////////////////////////
	if (m_drawSelectedAnimSkeleton)
	{
		char debugText[256];

		// Is the animation player and the animation valid?
		if (m_animSelector.GetSelectedClip())
		{

			float phase = m_drawSelectedAnimSkeletonPhase;
			const crClip* pClip = m_animSelector.GetSelectedClip();
			Vector3 vMoverTrackTrans(0.0f, 0.0f, 0.0f);
			Quaternion qMoverTrackRot(Quaternion::sm_I);

			if (m_drawSelectedAnimSkeletonDOFTest)
			{
				u8 track = m_drawSelectedAnimSkeletonDOFTestTrack;
				u8 dofType = m_drawSelectedAnimSkeletonDOFTestType;
				u16 id = m_drawSelectedAnimSkeletonDOFTestId;

				crFrameDataFixedDofs<1> frameDataDOFs;
				frameDataDOFs.AddDof(track, id, dofType);
				crFrameFixedDofs<1> frameDOFs(frameDataDOFs);
				pClip->Composite(frameDOFs, pClip->ConvertPhaseToTime(phase));

				bool bTrackFound = false;
				
				char tempText[256];

				switch (dofType)
				{
				case kFormatTypeVector3:
					{
						Vec3V vec;
						if (frameDOFs.GetValue<Vec3V>(track, id, vec))
						{
							bTrackFound = true;
							sprintf(tempText, "Vector DOF found - x: %.3f, y:%.3f, z:%.3f", vec.GetXf(), vec.GetYf(), vec.GetZf());
							grcDebugDraw::Text(VEC3V_TO_VECTOR3(m_pDynamicEntity->GetMatrix().d()), Color_white, 0, 0, tempText);
						}
					}
					break;
				case kFormatTypeQuaternion:
					{
						QuatV quat;
						if (frameDOFs.GetValue<QuatV>(track, id, quat))
						{
							bTrackFound = true;
							Vec3V eulers = QuatVToEulers(quat, EULER_XYZ);
							sprintf(tempText, "Quaternion DOF found - x: %.3f, y:%.3f, z:%.3f, w:%.3f (eulers x:%.3f, y:%.3f, z:%.3f)", quat.GetXf(), quat.GetYf(), quat.GetZf(), quat.GetWf(), eulers.GetXf(), eulers.GetYf(), eulers.GetZf());
							grcDebugDraw::Text(VEC3V_TO_VECTOR3(m_pDynamicEntity->GetMatrix().d()), Color_white, 0, 0, tempText);
						}
					}
					break;
				case kFormatTypeFloat:
					{
						float val;
						if (frameDOFs.GetValue<float>(track, id, val))
						{
							bTrackFound = true;
							sprintf(tempText, "Float DOF found - value: %.3f", val);
							grcDebugDraw::Text(VEC3V_TO_VECTOR3(m_pDynamicEntity->GetMatrix().d()), Color_white, 0, 0, tempText);
						}
					}
					break;
				}

				if (!bTrackFound)
				{
					grcDebugDraw::Text(VEC3V_TO_VECTOR3(m_pDynamicEntity->GetMatrix().d()), Color_white, 0, 0, "DOF not found");
				}
			}

			


			// Get the local space mover translation
			vMoverTrackTrans = fwAnimHelpers::GetMoverTrackTranslation(*pClip, phase);


			// Get the local space mover rotation
			// Only use the z component from the mover track (the x and y components will be applied to the root bone later)
			qMoverTrackRot = fwAnimHelpers::GetMoverTrackRotation(*pClip, phase);

			// moverSituation is the local space translation and rotation of the mover track	
			Matrix34 moverSituation;
			moverSituation.Identity();
			moverSituation.d = vMoverTrackTrans + Vector3(m_drawSelectedAnimSkeletonOffset, 0.0f, 0.0f);
			moverSituation.FromQuaternion(qMoverTrackRot);
			
			// Convert from local space moverSituation to local space parentMat
			Matrix34 localParentMat(M34_IDENTITY);
			localParentMat=moverSituation;

			Matrix34 globalParentMat(localParentMat);
			Matrix34 m = MAT34V_TO_MATRIX34(m_pDynamicEntity->GetMatrix());
			globalParentMat.Dot(m);

			int numberLines = 0;
			Vector3 a, b;
			for (int i = 0; i < totalBones; i++)
			{

				const crBoneData *boneData = m_pDynamicEntity->GetSkeleton()->GetSkeletonData().GetBoneData(i);
				//if (CPedBoneTagConvertor::GetIsBoneInMask( BONEMASK_BODYONLY, (eAnimBoneTag)boneData->GetBoneId()) )
				{					
					Matrix34 tempMat(M34_IDENTITY);

					fwAnimManager::GetObjectMatrix(&m_pDynamicEntity->GetSkeleton()->GetSkeletonData(), pClip, phase, boneData->GetBoneId(), tempMat);
					tempMat.Dot(globalParentMat);
					a.Set(tempMat.d);

					for (const crBoneData *child = boneData->GetChild(); child; child = child->GetNext())
					{
						fwAnimManager::GetObjectMatrix(&m_pDynamicEntity->GetSkeleton()->GetSkeletonData(), pClip, phase, child->GetBoneId(), tempMat);
						tempMat.Dot(globalParentMat);
						b.Set(tempMat.d);

						grcDebugDraw::Line(a, b, Color_white);
						numberLines++;
					}
				}				
			}

			const crBoneData *boneData = m_pDynamicEntity->GetSkeleton()->GetSkeletonData().GetBoneData(m_boneIndex);
			Matrix34 localAnimMat(M34_IDENTITY);
			fwAnimManager::GetLocalMatrix(&m_pDynamicEntity->GetSkeleton()->GetSkeletonData(), pClip, phase, boneData->GetBoneId(), localAnimMat);
			if (m_boneIndex==0)
			{
				localAnimMat.Dot(localParentMat);
			}

			Matrix34 globalAnimMat(M34_IDENTITY);
			fwAnimManager::GetObjectMatrix(&m_pDynamicEntity->GetSkeleton()->GetSkeletonData(), pClip, phase, boneData->GetBoneId(), globalAnimMat);
			globalAnimMat.Dot(globalParentMat);

			if (m_drawSelectedBoneMat)
			{
				grcDebugDraw::Axis(globalAnimMat, m_drawSelectedBoneMatSize, true);
			}

			// Output the selected bones local and global translation for the selected animation
			if (m_drawSelectedBoneMatTranslation)
			{
			
				///////////////////////////////////////////////////////////////////////////////
				//
				// Output the selected bones local and global translation (as eulers)  for the selected animation
				//
				Vector3 localTranslation = localAnimMat.d;
				Vector3 globalTranslation = globalAnimMat.d;

				sprintf(debugText,"Local Translation %7.4f, %7.4f, %7.4f \n", localTranslation.x, localTranslation.y, localTranslation.z);

				vTextRenderPos.y = m_fVerticalBorder  + (m_iNoOfTexts++)*m_fVertDist;
				grcDebugDraw::Text(vTextRenderPos, colour, debugText);

				sprintf(debugText,"Global Translation %7.4f, %7.4f, %7.4f \n", globalTranslation.x, globalTranslation.y, globalTranslation.z);

				vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
				grcDebugDraw::Text(vTextRenderPos, colour, debugText);
				///////////////////////////////////////////////////////////////////////////////
			}

			// Output the selected bones local and global rotation for the selected animation
			if (m_drawSelectedBoneMatRotation)
			{
				///////////////////////////////////////////////////////////////////////////////
				//
				// Output the selected bones local and global matrix (as eulers) for the selected animation
				//
				Vector3 localEuler = localAnimMat.GetEulers();
				Vector3 globalEuler = globalAnimMat.GetEulers();

				if (m_eulers)
				{
					if (m_degrees)
					{
						sprintf(debugText,"Local Rotation %7.4f, %7.4f, %7.4f \n", localEuler.x*RtoD, localEuler.y*RtoD, localEuler.z*RtoD);

						vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
						grcDebugDraw::Text(vTextRenderPos, colour, debugText);

						sprintf(debugText,"Global Rotation %7.4f, %7.4f, %7.4f \n", globalEuler.x*RtoD, globalEuler.y*RtoD, globalEuler.z*RtoD);

						vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
						grcDebugDraw::Text(vTextRenderPos, colour, debugText);
					}
					else
					{
						sprintf(debugText,"Local Rotation %7.4f, %7.4f, %7.4f \n", localEuler.x, localEuler.y, localEuler.z);

						vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
						grcDebugDraw::Text(vTextRenderPos, colour, debugText);

						sprintf(debugText,"Global Rotation %7.4f, %7.4f, %7.4f \n", globalEuler.x, globalEuler.y, globalEuler.z);

						vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
						grcDebugDraw::Text(vTextRenderPos, colour, debugText);
					}
				}
				else
				{
					///////////////////////////////////////////////////////////////////////////////
					//
					// Output the selected bones local and global matrix (as matrices) for the selected animation
					//
					vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
					grcDebugDraw::Text(vTextRenderPos, colour, "Local Rotation \n");

					sprintf(debugText, "%10.6f, %10.6f, %10.6f \n", localAnimMat.a.x, localAnimMat.a.y, localAnimMat.a.z);

					vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
					grcDebugDraw::Text(vTextRenderPos, colour, debugText);

					sprintf(debugText, "%10.6f, %10.6f, %10.6f \n", localAnimMat.b.x, localAnimMat.b.y, localAnimMat.b.z);

					vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
					grcDebugDraw::Text(vTextRenderPos, colour, debugText);

					sprintf(debugText, "%10.6f, %10.6f, %10.6f \n", localAnimMat.c.x, localAnimMat.c.y, localAnimMat.c.z);

					vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
					grcDebugDraw::Text(vTextRenderPos, colour, debugText);

					vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
					grcDebugDraw::Text(vTextRenderPos, colour, "Global Rotation \n");

					sprintf(debugText, "%10.6f, %10.6f, %10.6f \n", globalAnimMat.a.x, globalAnimMat.a.y, globalAnimMat.a.z);

					vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
					grcDebugDraw::Text(vTextRenderPos, colour, debugText);

					sprintf(debugText, "%10.6f, %10.6f, %10.6f \n", globalAnimMat.b.x, globalAnimMat.b.y, globalAnimMat.b.z);

					vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
					grcDebugDraw::Text(vTextRenderPos, colour, debugText);

					sprintf(debugText, "%10.6f, %10.6f, %10.6f \n", globalAnimMat.c.x, globalAnimMat.c.y, globalAnimMat.c.z);

					vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
					grcDebugDraw::Text(vTextRenderPos, colour, debugText);
					///////////////////////////////////////////////////////////////////////////////

				}
				///////////////////////////////////////////////////////////////////////////////

				vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
				grcDebugDraw::Text(vTextRenderPos, colour, "\n");
			}
		}
	}

	if (ms_bRenderEntityInfo)
	{
		RenderEntityInfo();
	}


	///////////////////////////////////////////////////////////////////////////////
	//
	// Render the selected entities skeleton ?
	//
	if (m_renderSkeleton)
	{
		Color32 skeletonColour = m_drawAllBonesMat ? Color_green1 : Color_yellow1;
		const crSkeleton* pSkeletonToRender = m_pDynamicEntity->GetSkeleton();

		// Render a line between the position of each child bone and its parent bone
		for (int i = 0; i < pSkeletonToRender->GetBoneCount(); i++)
		{
			const crBoneData *boneData = pSkeletonToRender->GetSkeletonData().GetBoneData(i);
			Mat34V mBone;
			pSkeletonToRender->GetGlobalMtx(i, mBone);

			Vec3V posA = mBone.d();
			for (const crBoneData *child = boneData->GetChild(); child; child = child->GetNext())
			{
				pSkeletonToRender->GetGlobalMtx(child->GetIndex(), mBone);
				Vec3V posB = mBone.d();
				grcDebugDraw::Line(posA, posB, skeletonColour);
			}
		}
	}

#if FPS_MODE_SUPPORTED
	///////////////////////////////////////////////////////////////////////////////
	//
	// Render the selected entities third-person skeleton ?
	//
	if (m_renderThirdPersonSkeleton)
	{
		Color32 skeletonColour = Color_SteelBlue4;

		const crSkeleton* pSkeletonToRender = NULL;
		if(m_pDynamicEntity->GetIsTypePed())
		{
			CPed *pPed = static_cast< CPed * >(m_pDynamicEntity.Get());
			if (pPed && pPed->GetIkManager().GetThirdPersonSkeleton())
			{
				pSkeletonToRender = pPed->GetIkManager().GetThirdPersonSkeleton();
			}

			if (pSkeletonToRender)
			{
				CPedWeaponManager* pWeaponManager = pPed->GetWeaponManager();

				if (pWeaponManager)
				{
					const CWeapon* pWeapon = pWeaponManager->GetEquippedWeapon();
					const CObject* pWeaponObject = pWeaponManager->GetEquippedWeaponObject();

					if (pWeapon && pWeaponObject)
					{
						const CWeaponModelInfo* pWeaponModelInfo = pWeapon->GetWeaponModelInfo();
						const crSkeleton* pWeaponSkeleton = pWeaponObject->GetSkeleton();
						int pointHelperBoneIdx = pPed->GetBoneIndexFromBoneTag(BONETAG_R_PH_HAND);

						if (pWeaponModelInfo && pWeaponSkeleton && (pointHelperBoneIdx >= 0))
						{
							Mat34V mtxWeapon;

							Mat34V mtxWeaponGrip(V_IDENTITY);
							s32 gripBoneIdx = pWeaponModelInfo->GetBoneIndex(WEAPON_GRIP_R);
							if (gripBoneIdx >= 0)
							{
								mtxWeaponGrip = pWeaponSkeleton->GetSkeletonData().GetDefaultTransform(gripBoneIdx);
							}

							Mat34V mtxPH;
							pSkeletonToRender->GetGlobalMtx(pointHelperBoneIdx, mtxPH);

							InvertTransformFull(mtxWeapon, mtxWeaponGrip);
							rage::Transform(mtxWeapon, mtxPH, mtxWeapon);

							grcDebugDraw::Axis(mtxWeapon, 0.10f, true);

							phInst* pInst = pWeaponObject->GetCurrentPhysicsInst();

							if (pInst)
							{
								phBound* pBound = pInst->GetArchetype()->GetBound();

								if (pBound)
								{
									PGTAMATERIALMGR->GetDebugInterface().UserRenderBound(pInst, pBound, mtxWeapon, false);
								}
							}
						}
					}
				}
			}
		}

		if (pSkeletonToRender)
		{
			// Render a line between the position of each child bone and its parent bone
			for (int i = 0; i < pSkeletonToRender->GetBoneCount(); i++)
			{
				const crBoneData *boneData = pSkeletonToRender->GetSkeletonData().GetBoneData(i);
				Mat34V mBone;
				pSkeletonToRender->GetGlobalMtx(i, mBone);

				Vec3V posA = mBone.d();
				for (const crBoneData *child = boneData->GetChild(); child; child = child->GetNext())
				{
					pSkeletonToRender->GetGlobalMtx(child->GetIndex(), mBone);
					Vec3V posB = mBone.d();
					grcDebugDraw::Line(posA, posB, skeletonColour);
					grcDebugDraw::Sphere(posB, 0.005f, Color_SteelBlue);
				}
			}
		}
	}
#endif // FPS_MODE_SUPPORTED

	///////////////////////////////////////////////////////////////////////////////
	//
	// Render the first person camera ?
	//
	if (m_drawFirstPersonCamera)
	{
		if(m_pDynamicEntity->GetIsTypePed())
		{
			CPed *pPed = static_cast< CPed * >(m_pDynamicEntity.Get());

			Vector2 modifiedTextRenderPos = vTextRenderPos;
			TUNE_GROUP_FLOAT(CAM_FPS_ANIMATED, fDebugTextRenderXPos, -1.0f, -1.0f, 1.0f, 0.001f);
			TUNE_GROUP_FLOAT(CAM_FPS_ANIMATED, fDebugTextRenderYPos, -1.0f, -1.0f, 1.0f, 0.001f);
			crSkeleton *pSkeleton = pPed->GetSkeleton();
			if(pSkeleton)
			{
				if(fDebugTextRenderXPos >= 0.0f)
				{
					modifiedTextRenderPos.x = fDebugTextRenderXPos;
				}
				if(fDebugTextRenderYPos >= 0.0f)
				{
					modifiedTextRenderPos.y = fDebugTextRenderYPos;
				}

				Color32 savedColour = colour;
				Vec3V vTrans(V_ZERO);
				QuatV qRot(V_IDENTITY);
				float fMinX = 0.0f, fMaxX = 0.0f, fMinY = 0.0f, fMaxY = 0.0f, fMinZ = 0.0f, fMaxZ = 0.0f, fWeight = 0.0f, fov = 0.0f;
				bool bFoundAnyDOFs = pPed->GetFirstPersonCameraDOFs(vTrans, qRot, fMinX, fMaxX, fMinY, fMaxY, fMinZ, fMaxZ, fWeight, fov);

				fMinX *= DtoR; fMinY *= DtoR; fMinZ *= DtoR;
				fMaxX *= DtoR; fMaxY *= DtoR; fMaxZ *= DtoR;

				bool bAllowedToReadDOFs = true;
				FPS_MODE_SUPPORTED_ONLY(camFirstPersonShooterCamera* pFpsCamera = camInterface::GetGameplayDirector().GetFirstPersonShooterCamera();)
				FPS_MODE_SUPPORTED_ONLY(bAllowedToReadDOFs = (pFpsCamera && (pFpsCamera->IsUsingAnimatedData() || pFpsCamera->IsAnimatedCameraUpdatingInBlendOut()));)
				colour.Set(200, 200, 200, (int)savedColour.GetAlpha());		// inactive colour
				bFoundAnyDOFs &= bAllowedToReadDOFs;						// consider inactive if first person camera is not able to read the values.
				if (bFoundAnyDOFs)
				{
					colour = savedColour;
				#if FPS_MODE_SUPPORTED
					if(pFpsCamera && !pFpsCamera->IsUsingAnimatedData())
					{
						colour.Set(255, 255, 0, (int)savedColour.GetAlpha());		// animated camera being updated during blend out
					}
				#endif
				}

				// Ped

				////int rootIdx = 0;
				////pPed->GetSkeletonData().ConvertBoneIdToIndex(BONETAG_ROOT, rootIdx);
				////pSkeleton->GetGlobalMtx(rootIdx, mPed);
				Mat34V mPed = pPed->GetTransform().GetMatrix();

				Mat34V mLocalHead;
				int headIdx = 0;
				pPed->GetSkeletonData().ConvertBoneIdToIndex(BONETAG_HEAD, headIdx);
				mLocalHead = pSkeleton->GetObjectMtx(headIdx);

				// Transform local head position by ped matrix to get world space offset.
				Vec3V vAttachPosition = Transform3x3(mPed, mLocalHead.GetCol3());
				mPed.SetCol3( mPed.GetCol3() + vAttachPosition );

				// Cam - offset is now relative to head bone.

				Mat34V mCam; Mat34VFromQuatV(mCam, qRot, vTrans);
				Transform(mCam, mPed, mCam);

			#if FPS_MODE_SUPPORTED
				if(pFpsCamera)
				{
					Mat34V mFpsCamera = MATRIX34_TO_MAT34V(pFpsCamera->GetFrame().GetWorldMatrix());
					Vec3V  vOffset;
					vOffset = Transform3x3(mFpsCamera, vTrans);
					mCam.SetCol3( mPed.GetCol3() + vOffset);
				}
			#endif

				//

				sprintf(debugText, "FirstPersonCam %s   weight %.3f", bFoundAnyDOFs ? "ACTIVE" : "inactive", fWeight);
				modifiedTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
				grcDebugDraw::Text(modifiedTextRenderPos, colour, debugText);

				//

				sprintf(debugText, "Trans %10.6f, %10.6f, %10.6f", vTrans.GetXf(), vTrans.GetYf(), vTrans.GetZf());
				modifiedTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
				grcDebugDraw::Text(modifiedTextRenderPos, colour, debugText);


				//

				Vec3V vRotEulers = QuatVToEulers(qRot, EULER_XYZ);
				sprintf(debugText, "Rot   %10.6f, %10.6f, %10.6f", vRotEulers.GetXf() * RtoD, vRotEulers.GetYf() * RtoD, vRotEulers.GetZf() * RtoD);
				modifiedTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
				grcDebugDraw::Text(modifiedTextRenderPos, colour, debugText);

				//

				sprintf(debugText, "MinX %10.6f  MaxX %10.6f", fMinX * RtoD, fMaxX * RtoD);
				if(fov > SMALL_FLOAT)
				{
					char szTmp[16];
					sprintf(szTmp, "   fov %3.2f", fov);
					safecat(debugText, szTmp, NELEM(debugText));
				}
				modifiedTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
				grcDebugDraw::Text(modifiedTextRenderPos, colour, debugText);

				//

				sprintf(debugText, "MinY %10.6f  MaxY %10.6f", fMinY * RtoD, fMaxY * RtoD);
				modifiedTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
				grcDebugDraw::Text(modifiedTextRenderPos, colour, debugText);

				//

				sprintf(debugText, "MinZ %10.6f  MaxZ %10.6f", fMinZ * RtoD, fMaxZ * RtoD);
				modifiedTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
				grcDebugDraw::Text(modifiedTextRenderPos, colour, debugText);

			#if FPS_MODE_SUPPORTED
				if( pFpsCamera && (pFpsCamera->IsUsingAnimatedData() || pFpsCamera->IsAnimatedCameraDataBlendingOut()) )
				{
					sprintf(debugText, "Blended MinX %10.6f  MaxX %10.6f",  pFpsCamera->GetAnimatedSpringPitchLimits().x,    pFpsCamera->GetAnimatedSpringPitchLimits().y);
					modifiedTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
					grcDebugDraw::Text(modifiedTextRenderPos, colour, debugText);

					sprintf(debugText, "Blended MinY %10.6f  MaxY %10.6f", -pFpsCamera->GetAnimatedSpringHeadingLimits().y, -pFpsCamera->GetAnimatedSpringHeadingLimits().x);
					modifiedTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
					grcDebugDraw::Text(modifiedTextRenderPos, colour, debugText);

					sprintf(debugText, "Camera relative heading %10.6f  pitch %10.6f", 
										(pFpsCamera->GetRelativeHeading(true) + pFpsCamera->GetMeleeRelativeHeading()) * RtoD,
										(pFpsCamera->GetRelativePitch(true)   + pFpsCamera->GetMeleeRelativePitch()  ) * RtoD);
					modifiedTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
					grcDebugDraw::Text(modifiedTextRenderPos, colour, debugText);
				}
			#endif

				fwAnimDirector* pAnimDirector = pPed->GetAnimDirector();
				if(pAnimDirector)
				{
					const CClipEventTags::CFirstPersonCameraEventTag* pPropCamera =
						static_cast<const CClipEventTags::CFirstPersonCameraEventTag*>(pAnimDirector->FindPropertyInMoveNetworks(CClipEventTags::FirstPersonCamera));
					if(pPropCamera)
					{
						if(pPropCamera->GetAllowed())
						{
							float duration = pPropCamera->GetBlendInDuration();
							sprintf(debugText, "Allowed   Blend IN %.3f seconds", duration);
							modifiedTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
							grcDebugDraw::Text(modifiedTextRenderPos, colour, debugText);
						}
						else
						{
							float duration = pPropCamera->GetBlendOutDuration();
							sprintf(debugText, "Blocked   Blend OUT %.3f seconds", duration);
							modifiedTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
							grcDebugDraw::Text(modifiedTextRenderPos, colour, debugText);
						}
					}

					const CClipEventTags::CFirstPersonCameraInputEventTag* pPropInput =
						static_cast<const CClipEventTags::CFirstPersonCameraInputEventTag*>(pAnimDirector->FindPropertyInMoveNetworks(CClipEventTags::FirstPersonCameraInput));
					if(pPropInput)
					{
						bool bAnimatedYawSpringInput   = pPropInput->GetYawSpringInput();
						bool bAnimatedPitchSpringInput = pPropInput->GetPitchSpringInput();
						sprintf(debugText, "Yaw Input %s   Pitch Input %s", (bAnimatedYawSpringInput) ? "spring" : "full", (bAnimatedPitchSpringInput) ? "spring" : "full");
						modifiedTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
						grcDebugDraw::Text(modifiedTextRenderPos, colour, debugText);
					}
				}

				if(bAllowedToReadDOFs)
				{
					grcDebugDraw::Axis(mCam, m_drawSelectedBoneMatSize, true);

					Vec3V vOrigin(0.0f, 0.0f, 0.0f);
					vOrigin = Transform(mCam, vOrigin);

					float fDepth = 1.0f;
					Vec3V vCenter(0.0f, fDepth, 0.0f);
					vCenter = Transform(mCam, vCenter);

					// MaxX is max pitch limit, MinX is min pitch limit
					// -MaxY is left heading limit, -MinY is right heading limit
					Vec3V vMinTopLeft    (tanf(-fMaxY) * fDepth, fDepth, tanf(fMaxX) * fDepth);
					Vec3V vMinTopRight   (tanf(-fMinY) * fDepth, fDepth, tanf(fMaxX) * fDepth);
					Vec3V vMinBottomRight(tanf(-fMinY) * fDepth, fDepth, tanf(fMinX) * fDepth);
					Vec3V vMinBottomLeft (tanf(-fMaxY) * fDepth, fDepth, tanf(fMinX) * fDepth);
					vMinTopLeft     = Transform(mCam, vMinTopLeft    );
					vMinTopRight    = Transform(mCam, vMinTopRight   );
					vMinBottomRight = Transform(mCam, vMinBottomRight);
					vMinBottomLeft  = Transform(mCam, vMinBottomLeft );
					grcDebugDraw::Quad(vMinTopLeft, vMinTopRight, vMinBottomRight, vMinBottomLeft, Color_white, true, false);
					grcDebugDraw::Line(vOrigin, vMinTopLeft    , Color_white);
					grcDebugDraw::Line(vOrigin, vMinTopRight   , Color_white);
					grcDebugDraw::Line(vOrigin, vMinBottomRight, Color_white);
					grcDebugDraw::Line(vOrigin, vMinBottomLeft , Color_white);

					Vec3V vMinZ(0.0f, fDepth, fDepth / 2.0f);
					ScalarV sMinZ(fMinZ);
					vMinZ = RotateAboutYAxis(vMinZ, sMinZ);
					vMinZ = Transform(mCam, vMinZ);
					grcDebugDraw::Line(vCenter, vMinZ, Color_white);

					Vec3V vMaxZ(0.0f, fDepth, fDepth / 2.0f);
					ScalarV sMaxZ(fMaxZ);
					vMaxZ = RotateAboutYAxis(vMaxZ, sMaxZ);
					vMaxZ = Transform(mCam, vMaxZ);
					grcDebugDraw::Line(vCenter, vMaxZ, Color_white);
				}

				if(modifiedTextRenderPos.x == vTextRenderPos.x)
				{
					vTextRenderPos.y = modifiedTextRenderPos.y;
				}
				colour = savedColour;		// restore
			}
		}
	}

	///////////////////////////////////////////////////////////////////////////////
	//
	// Render hair
	//
	///////////////////////////////////////////////////////////////////////////////

	if (m_drawSelectedEntityHair)
	{
		if (m_pDynamicEntity->GetIsTypePed())
		{
			CPed* pPed = static_cast<CPed*>(m_pDynamicEntity.Get());

			crSkeleton* pSkeleton = pPed->GetSkeleton();
			if (pSkeleton)
			{
				const crSkeletonData &skeletonData = pSkeleton->GetSkeletonData();

				int headIdx = -1;
				skeletonData.ConvertBoneIdToIndex(BONETAG_HEAD, headIdx);
				Mat34V mHead; pSkeleton->GetGlobalMtx(headIdx, mHead);

				Vec3V vHead(V_ZERO);
				vHead = Transform(mHead, vHead);

				Vec3V vHair(V_UP_AXIS_WZERO);
				vHair = Scale(vHair, ScalarV(pPed->GetHairHeight()));
				vHair = vHead + vHair;

				grcDebugDraw::Line(vHead, vHair, Color_white);
				grcDebugDraw::Cross(vHair, 0.1f, Color_white);

				if (m_drawSelectedEntityHairMeasure)
				{
					Vec3V vHairMeasure(V_UP_AXIS_WZERO);
					vHairMeasure = Scale(vHairMeasure, ScalarV(m_drawSelectedEntityHairMeasureHeight));
					vHairMeasure = vHead + vHairMeasure;

					grcDebugDraw::Line(vHead, vHairMeasure, Color_red);
					grcDebugDraw::Cross(vHairMeasure, 0.1f, Color_red);
				}
			}
		}
	}

	///////////////////////////////////////////////////////////////////////////////
	//
	// Render constraint helper DOFs 
	//
	if (m_drawConstraintHelpers)
	{
		if (m_pDynamicEntity->GetIsTypePed())
		{
			CPed* pPed = static_cast<CPed*>(m_pDynamicEntity.Get());
			crSkeleton* pSkeleton = pPed->GetSkeleton();

			for (int helper = 0; helper < 2; ++helper)
			{
				Mat34V mtxHelper(V_IDENTITY);
				Mat34V mtxHelperOS(V_IDENTITY);
				Mat34V mtxHelperGS(V_IDENTITY);
				Vec3V vTranslation;
				QuatV qRotation;
				float fWeight;

				Mat34V mtxHand(V_IDENTITY);
				const bool bRight = (helper == 0);
				eAnimBoneTag boneTag = bRight ? BONETAG_R_HAND : BONETAG_L_HAND;
				s32 handBoneIdx = pPed->GetBoneIndexFromBoneTag(boneTag);
				if (handBoneIdx != -1)
				{
					mtxHand = pSkeleton->GetObjectMtx(handBoneIdx);
				}

				bool bValid = pPed->GetConstraintHelperDOFs(bRight, fWeight, vTranslation, qRotation);
				if (bValid)
				{
					Mat34VFromQuatV(mtxHelper, qRotation, vTranslation);
				}

				sprintf(debugText,"%s\n", bRight ? "BONETAG_CH_R_HAND" : "BONETAG_CH_L_HAND");
				vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
				grcDebugDraw::Text(vTextRenderPos, colour, debugText);

				sprintf(debugText,"ID = %u\n", bRight ? (u16)BONETAG_CH_R_HAND : (u16)BONETAG_CH_L_HAND);
				vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
				grcDebugDraw::Text(vTextRenderPos, colour, debugText);

				sprintf(debugText,"Weight %5.3f\n", fWeight);
				vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
				grcDebugDraw::Text(vTextRenderPos, colour, debugText);

				sprintf(debugText,"Local Translation %7.4f, %7.4f, %7.4f\n", mtxHelper.GetCol3().GetXf(), mtxHelper.GetCol3().GetYf(), mtxHelper.GetCol3().GetZf());
				vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
				grcDebugDraw::Text(vTextRenderPos, colour, debugText);

				Transform(mtxHelperOS, mtxHand, mtxHelper);
				sprintf(debugText,"Object Translation %7.4f, %7.4f, %7.4f\n", mtxHelperOS.GetCol3().GetXf(), mtxHelperOS.GetCol3().GetYf(), mtxHelperOS.GetCol3().GetZf());
				vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
				grcDebugDraw::Text(vTextRenderPos, colour, debugText);

				Transform(mtxHelperGS, *pSkeleton->GetParentMtx(), mtxHelperOS);
				sprintf(debugText,"Global Translation %7.4f, %7.4f, %7.4f \n", mtxHelperGS.GetCol3().GetXf(), mtxHelperGS.GetCol3().GetYf(), mtxHelperGS.GetCol3().GetZf());
				vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
				grcDebugDraw::Text(vTextRenderPos, colour, debugText);

				Vec3V vEulers(Mat34VToEulersXYZ(mtxHelper));
				sprintf(debugText,"Local Rotation %7.4f, %7.4f, %7.4f \n", vEulers.GetXf() * RtoD, vEulers.GetYf() * RtoD, vEulers.GetZf() * RtoD);
				vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
				grcDebugDraw::Text(vTextRenderPos, colour, debugText);

				vEulers = Mat34VToEulersXYZ(mtxHelperOS);
				sprintf(debugText,"Object Rotation %7.4f, %7.4f, %7.4f \n", vEulers.GetXf() * RtoD, vEulers.GetYf() * RtoD, vEulers.GetZf() * RtoD);
				vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
				grcDebugDraw::Text(vTextRenderPos, colour, debugText);

				vEulers = Mat34VToEulersXYZ(mtxHelperGS);
				sprintf(debugText,"Global Rotation %7.4f, %7.4f, %7.4f \n", vEulers.GetXf() * RtoD, vEulers.GetYf() * RtoD, vEulers.GetZf() * RtoD);
				vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
				grcDebugDraw::Text(vTextRenderPos, colour, debugText);

				grcDebugDraw::Axis(mtxHelperGS, m_drawSelectedBoneMatSize, true);
			}
		}
	}

	//bone history rendering
	if (m_autoAddBoneHistoryEveryFrame)
	{
		//don't do this if the game's paused
		if (!fwTimer::IsUserPaused() || fwTimer::GetSingleStepThisFrame())
		{
			AddBoneHistoryEntry();
		}
	}
	if (m_renderBoneHistory)
	{
		RenderBoneHistory();
	}

	if (ms_bRenderCameraCurve)
	{
		RenderCameraCurve();
	}

	// Render the old CAnimDebug stuff
	OldDebugRender();
}

void CAnimViewer::RenderBoneHistory()
{
	// Render the selected bone and mover track history (as red and blue lines)
	// to show translation

	for(int i=1; i<m_boneHistoryArray.GetCount(); i++)
	{
		int boneEntry = (i+m_matIndex)%m_boneHistoryArray.GetCount();
		int lastEntry = boneEntry - 1;
		
		if (lastEntry == -1)
			lastEntry = m_boneHistoryArray.GetCount()-1;

		if ( boneEntry > 0 && boneEntry<m_boneHistoryArray.GetCount() && lastEntry > 0 && lastEntry<m_boneHistoryArray.GetCount())
		{
			float phase = m_boneHistoryArray[lastEntry].m_phase;
			float oneMinusphase = 1.0f - phase;
			Color32 color(Color32( int(255 * phase), 0, int(255 * oneMinusphase) ) );
			grcDebugDraw::Line(m_boneHistoryArray[lastEntry].m_boneMatrix.d, m_boneHistoryArray[boneEntry].m_boneMatrix.d, color);

			color = (Color32( int(255 * phase), int(255 * oneMinusphase), 0 ) );
			grcDebugDraw::Line(m_boneHistoryArray[lastEntry].m_moverMatrix.d, m_boneHistoryArray[boneEntry].m_moverMatrix.d, color);
		}
	}

	// Render the selected bone and mover track history (as axis)
	// to show orientation
	for(int i=0; i<m_boneHistoryArray.GetCount(); i++)
	{
		grcDebugDraw::Axis(m_boneHistoryArray[i].m_boneMatrix, m_drawAllBonesMatSize, true);
		grcDebugDraw::Axis(m_boneHistoryArray[i].m_moverMatrix, m_drawAllBonesMatSize, true);
		// Draw a line between the root and the mover
		grcDebugDraw::Line(m_boneHistoryArray[i].m_boneMatrix.d,m_boneHistoryArray[i].m_moverMatrix.d, Color_white);
	}
}

void CAnimViewer::OnDynamicEntityPreRender()
{
	if(m_bDumpPose && animVerifyf(m_pDynamicEntity, "No entity selected!"))
	{
		USE_DEBUG_ANIM_MEMORY();

		fwMoveDumpNetwork dump(m_pDynamicEntity.Get(), true, NULL, false);

		if(m_DumpPoseFrameIndex <= m_DumpPoseFrameCount)
		{
			const crClip *pClip = m_DumpPoseClipHelper.GetClip();

			// Write frame element (open)
			DumpPosePrint("<frame index=\"%u\" time=\"%.3f\">", m_DumpPoseFrameIndex, pClip->ConvertPhaseToTime(static_cast< float >(m_DumpPoseFrameIndex) / static_cast< float >(m_DumpPoseFrameCount)));
			m_DumpPosePrintIndent ++;

			const atArray< fwMoveDumpDof * > &dofs = dump.GetDofs();
			for(int i = 0; i < dofs.GetCount(); i ++)
			{
				fwMoveDumpDof *pDof = dofs[i];
				if(pDof)
				{
					if(!m_bDumpPosePrintOnlyDOFsInClip || pClip->HasDof(pDof->GetTrack(), pDof->GetId()))
					{
						switch(pDof->GetDofType())
						{
						case eDofUnknown:
							{
								DumpPosePrint("<dof track=\"%u\" trackname=\"%s\" id=\"%u\" idname=\"%s\" valid=\"%s\"/>", pDof->GetTrack(), pDof->GetTrackName(), pDof->GetId(), pDof->GetIdName(), pDof->GetInvalid() ? "false" : "true");
							} break;
						case eDofVector3:
							{
								fwMoveDumpDofVector3 *pDofVec3 = static_cast< fwMoveDumpDofVector3 * >(pDof);
								DumpPosePrint("<dofvec3 track=\"%u\" trackname=\"%s\" id=\"%u\" idname=\"%s\" valid=\"%s\" x=\"%.3f\" y=\"%.3f\" z=\"%.3f\"/>", pDof->GetTrack(), pDof->GetTrackName(), pDof->GetId(), pDof->GetIdName(), pDof->GetInvalid() ? "false" : "true", pDofVec3->GetValue().GetX(), pDofVec3->GetValue().GetY(), pDofVec3->GetValue().GetZ());
							} break;
						case eDofQuaternion:
							{
								fwMoveDumpDofQuaternion *pDofQuat = static_cast< fwMoveDumpDofQuaternion * >(pDof);
								if(m_bDumpPosePrintEulers && !pDof->GetInvalid())
								{
									Vector3 eulers; pDofQuat->GetValue().ToEulers(eulers);
									DumpPosePrint("<dofquat track=\"%u\" trackname=\"%s\" id=\"%u\" idname=\"%s\" valid=\"%s\" x=\"%.3f\" y=\"%.3f\" z=\"%.3f\" w=\"%.3f\" ex=\"%.3f\" ey=\"%.3f\" ez=\"%.3f\"/>", pDof->GetTrack(), pDof->GetTrackName(), pDof->GetId(), pDof->GetIdName(), pDof->GetInvalid() ? "false" : "true", pDofQuat->GetValue().x, pDofQuat->GetValue().y, pDofQuat->GetValue().z, pDofQuat->GetValue().w, eulers.GetX() * RtoD, eulers.GetY() * RtoD, eulers.GetZ() * RtoD);
								}
								else
								{
									DumpPosePrint("<dofquat track=\"%u\" trackname=\"%s\" id=\"%u\" idname=\"%s\" valid=\"%s\" x=\"%.3f\" y=\"%.3f\" z=\"%.3f\" w=\"%.3f\"/>", pDof->GetTrack(), pDof->GetTrackName(), pDof->GetId(), pDof->GetIdName(), pDof->GetInvalid() ? "false" : "true", pDofQuat->GetValue().x, pDofQuat->GetValue().y, pDofQuat->GetValue().z, pDofQuat->GetValue().w);
								}
							} break;
						case eDofFloat:
							{
								fwMoveDumpDofFloat *pDofFloat = static_cast< fwMoveDumpDofFloat * >(pDof);
								DumpPosePrint("<doffloat track=\"%u\" trackname=\"%s\" id=\"%u\" idname=\"%s\" valid=\"%s\" value=\"%.3f\"/>", pDof->GetTrack(), pDof->GetTrackName(), pDof->GetId(), pDof->GetIdName(), pDof->GetInvalid() ? "false" : "true", pDofFloat->GetValue());
							} break;
						}
					}
				}
			}

			// Write frame element (close)
			m_DumpPosePrintIndent --;
			DumpPosePrint("</frame>");

			m_DumpPoseFrameIndex ++;
			m_DumpPoseClipHelper.SetPhase(static_cast< float >(m_DumpPoseFrameIndex) / static_cast< float >(m_DumpPoseFrameCount));
		}

		if(m_DumpPoseFrameIndex > m_DumpPoseFrameCount)
		{
			m_DumpPoseFrameIndex = 0;
			m_DumpPoseFrameCount = 0;

			// Write animation element (close)
			m_DumpPosePrintIndent --;
			DumpPosePrint("</animation>");

			animAssertf(m_DumpPosePrintIndent == 0, "");
			m_DumpPosePrintIndent = 0;

			m_pDumpPoseFile->Close(); m_pDumpPoseFile = NULL;

			m_bDumpPose = false;

			m_DumpPoseClipHelper.BlendOutClip(INSTANT_BLEND_OUT_DELTA);

			ASSET.PopFolder();

			animDisplayf("Pose dumped.");
		}
	}

	if(fwTimer::GetFrameCount() >= m_uNextApplyCaptureDofsPreRenderFrame)
	{
		if(!m_bCaptureMotionTree && !m_bCapturingMotionTree && m_bApplyCapturedDofs)
		{
			u32 uFrameIndex = Min(m_uRenderMotionTreeCaptureFrame, m_MoveDump->GetFrameCount() - 1);

			if(m_pDynamicEntity)
			{
				if(m_pDynamicEntity->GetIsTypePed())
				{
					// Force low lod physics by default (this removes collision and 
					static_cast< CPed * >(m_pDynamicEntity.Get())->GetPedAiLod().SetForcedLodFlag(CPedAILod::AL_LodPhysics);
				}

				const char *szFilter = NULL;
				if(m_iCaptureFilterNameIndex >= 0 && m_iCaptureFilterNameIndex < m_iCaptureFilterNameCount)
				{
					szFilter = m_szCaptureFilterNames[m_iCaptureFilterNameIndex];
				}
				else
				{
					szFilter = "BONEMASK_LOD_LO";
				}
				m_MoveDump->Apply(m_pDynamicEntity, uFrameIndex, szFilter, m_bRelativeToParent);
			}
		}

		m_uNextApplyCaptureDofsPreRenderFrame = fwTimer::GetFrameCount() + 1;
	}
}

void CAnimViewer::OutputBoneHistory()
{
	// Outputs the contents of the bone history array to a file

	// open the csv file
	char 		pLine[255];
	FileHandle	fid;

	// Try and open a file for the results
	CFileMgr::SetDir("common:/DATA");
	fid = CFileMgr::OpenFileForWriting("BoneHistory.csv");
	CFileMgr::SetDir("");

	if ( CFileMgr::IsValidFileHandle(fid) )
	{	

		animDisplayf("Starting output of bone history\n");

		sprintf(pLine, "Bone,,,,,,,Mover\nPosX,PosY,PosZ,EulX,EulY,EulZ,,PosX,PosY,PosZ,EulX,EulY,EulZ\n");
#if __DEV
		animAssert(CFileMgr::Write(fid, &pLine[0], istrlen(pLine))>0);
#else // __DEV
		CFileMgr::Write(fid, &pLine[0], istrlen(pLine));
#endif // __DEV

		// for each entry
		for(int i=1; i<m_boneHistoryArray.GetCount(); i++)
		{
			int boneEntry = (i+m_matIndex)%m_boneHistoryArray.GetCount();

			Vector3 boneEuler, moverEuler;
			// write the position and euler orientation
			m_boneHistoryArray[boneEntry].m_boneMatrix.ToEulersXYZ(boneEuler);
			m_boneHistoryArray[boneEntry].m_moverMatrix.ToEulersXYZ(moverEuler);

			sprintf(pLine, "%f,%f,%f,%f,%f,%f,,%f,%f,%f,%f,%f,%f\n", 
				m_boneHistoryArray[boneEntry].m_boneMatrix.d.x, 
				m_boneHistoryArray[boneEntry].m_boneMatrix.d.y, 
				m_boneHistoryArray[boneEntry].m_boneMatrix.d.z,
				boneEuler.x,
				boneEuler.y,
				boneEuler.z,
				m_boneHistoryArray[boneEntry].m_moverMatrix.d.x, 
				m_boneHistoryArray[boneEntry].m_moverMatrix.d.y, 
				m_boneHistoryArray[boneEntry].m_moverMatrix.d.z,
				moverEuler.x,
				moverEuler.y,
				moverEuler.z);

#if __DEV
			animAssert(CFileMgr::Write(fid, &pLine[0], istrlen(pLine))>0);
#else // __DEV
			CFileMgr::Write(fid, &pLine[0], istrlen(pLine));
#endif //__DEV
		}

		// save file
		if ( CFileMgr::IsValidFileHandle(fid) )
			CFileMgr::CloseFile(fid);

		animDisplayf("%d lines saved to common:/DATA/BoneHistory.csv\n", m_matIndex);
	}
	else
	{
		animDisplayf("BoneHistory.csv is read only");
		grcDebugDraw::PrintToScreenCoors("BoneHistory.csv is read only", 5, 6);
	}

}


#if __BANK
void CAnimViewer::InitLevelWidgets()
{
	if (AssertVerify(!m_pBank))
	{
		m_pBank = fwDebugBank::CreateBank("Animation", MakeFunctor(CAnimViewer::ActivateBank), MakeFunctor(CAnimViewer::DeactivateBank), MakeFunctor(CAnimViewer::CreatePermanentWidgets));
		//m_pBank = &BANKMGR.CreateBank("Animation", 100, 100, false);
	}
}

void CAnimViewer::CreatePermanentWidgets(fwDebugBank* pBank)
{
	pBank->AddToggle("Visualise motion tree", &m_bVisualiseMotionTree, ToggleMotionTreeFull, "");
	bkCombo* pCombo = pBank->AddCombo("Visualise motion tree phase", (int*)&m_eVisualiseMotionTreePhase, fwAnimDirectorComponent::kPhaseNum, NULL);
	if (pCombo != NULL)
	{
		pCombo->SetString(fwAnimDirectorComponent::kPhasePrePhysics, "Pre Physics");
		pCombo->SetString(fwAnimDirectorComponent::kPhaseMidPhysics, "Mid Physics");
		pCombo->SetString(fwAnimDirectorComponent::kPhasePreRender, "Pre Render");
	}
	pBank->AddToggle("Capture motion tree", &m_bCaptureMotionTree);
	pBank->AddSlider("Debug x indent", &g_motionTreeVisualisationXIndent, -10.0f, 0.035f, 0.005f);
	pBank->AddSlider("Debug y indent", &g_motionTreeVisualisationYIndent, -20.0f, 0.035f, 0.005f);
	pBank->AddSlider("Debug x increment", &g_motionTreeVisualisationXIncrement, -1.0f, 1.00f, 0.005f);
	pBank->AddSlider("Debug y increment", &g_motionTreeVisualisationYIncrement, -1.0f, 1.00f, 0.005f);
	fwMotionTreeVisualiser::AddWidgets(pBank);
}

void CAnimViewer::ToggleMotionTreeFull()
{
	ActivatePicker();
	if(m_bVisualiseMotionTree)
	{
		m_bVisualiseMotionTreeSimple = false;
	}
}

void CAnimViewer::ToggleMotionTreeSimple()
{
	ActivatePicker();
	if(m_bVisualiseMotionTreeSimple)
	{
		m_bVisualiseMotionTree = false;
	}
}

void CAnimViewer::ActivatePicker()
{
	g_PickerManager.SetEnabled(true);
	g_PickerManager.SetUiEnabled(false);
}

class CAnimViewerVCRHelper
{
public:

	void GoToStartAnim() { CAnimViewer::GoToStartAnim(); }
	void StepForwardAnim() { CAnimViewer::StepForwardAnim(); }
	void PlayForwardsAnim() { CAnimViewer::PlayForwardsAnim(); }
	void PauseAnim() { CAnimViewer::PauseAnim(); }
	void PlayBackwardsAnim() { CAnimViewer::PlayBackwardsAnim(); }
	void StepBackwardAnim() { CAnimViewer::StepBackwardAnim(); }
	void GoToEndAnim() { CAnimViewer::GoToEndAnim(); }
} animViewerVCRHelper;

void CAnimViewer::ActivateBank()
{
	m_active =  true;

	// Block ambient idles - wait until the bank is activated before doing this.
	CTaskAmbientClips::BLOCK_ALL_AMBIENT_IDLES = true;

#if __DEV
	fwAnimDirector::SetForceLoadAnims(true);
#endif // __DEV

	if(!m_pDynamicEntity)
	{
		m_pDynamicEntity = FindPlayerPed();
	}

	//automatically init the picker interface for entity selection
	ActivatePicker();

	CDebugClipDictionary::LoadClipDictionaries();

	// Only do this when the anim viewer is toggled to active
	InitModelNameCombo();
#if USE_PM_STORE
	//InitPmDictNameCombo();
#endif // USE_PM_STORE
	InitBoneNames();
	InitSpringNames();

	m_pBank->PushGroup("Capture"); 
	m_pBank->PushGroup("Motion Tree Capture"); 
		m_pBank->AddToggle("Capture DoFs", &m_bCaptureDofs);
		m_pBank->AddToggle("Track largest ped motion tree", &m_bTrackLargestMotionTree, NullCB, "Automatically select and capture the the largest ped motion tree every frame.");
		m_pBank->AddSlider("Auto pause complexity", &fwMotionTreeTester::ms_autoPauseComplexity, 0, 800, 1);
		m_pBank->AddSlider("Current complexity high watermark", &fwMotionTreeTester::ms_maxComplexity, 0, 800, 1);
		m_pBank->AddSlider("Auto pause depth", &fwMotionTreeTester::ms_autoPauseTreeDepth, 0, 128, 1);
		m_pBank->AddSlider("Current depth high watermark", &fwMotionTreeTester::ms_maxTreeDepth, 0, 128, 1);

		if(m_iCaptureFilterNameCount == 0)
		{
				strLocalIndex iFrameFilterDictionarySlot = strLocalIndex(g_FrameFilterDictionaryStore.FindSlot(FILTER_SET_PLAYER));
			if(g_FrameFilterDictionaryStore.IsValidSlot(iFrameFilterDictionarySlot))
			{
				crFrameFilterDictionary *pFilterDictionary = g_FrameFilterDictionaryStore.Get(iFrameFilterDictionarySlot);
				if(pFilterDictionary)
				{
					for(int i = 0; i < pFilterDictionary->GetCount(); i ++)
					{
						atHashString filterNameHash = pFilterDictionary->GetCode(i);
						const char *szFilterName = filterNameHash.TryGetCStr();
						if(szFilterName)
						{
							int iFilterNameLength = istrlen(szFilterName);
							if(iFilterNameLength > 0)
							{
								m_iCaptureFilterNameCount ++;
							}
						}
					}

					m_iCaptureFilterNameCount ++;

					m_szCaptureFilterNames = rage_new const char *[m_iCaptureFilterNameCount];

					for(int i = 0, j = 0; i < pFilterDictionary->GetCount(); i ++)
					{
						atHashString filterNameHash = pFilterDictionary->GetCode(i);
						const char *szFilterName = filterNameHash.TryGetCStr();
						if(szFilterName)
						{
							int iFilterNameLength = istrlen(szFilterName);
							if(iFilterNameLength > 0)
							{
								char *szFilterNameCopy = rage_new char[iFilterNameLength + 1];
								strcpy(szFilterNameCopy, szFilterName);
								m_szCaptureFilterNames[j ++] = szFilterNameCopy;
							}
						}
					}

					{
						const char *szFilterName = "(null filter)";
						int iFilterNameLength = istrlen(szFilterName);
						char *szFilterNameCopy = rage_new char[iFilterNameLength + 1];
						strcpy(szFilterNameCopy, szFilterName);
						m_szCaptureFilterNames[m_iCaptureFilterNameCount - 1] = szFilterNameCopy;
					}

					// Sort the model names alphabetically
					std::sort(&m_szCaptureFilterNames[0], &m_szCaptureFilterNames[m_iCaptureFilterNameCount], AlphabeticalSort());

					for(int i = 0; i < m_iCaptureFilterNameCount; i ++)
					{
						if(stricmp(m_szCaptureFilterNames[i], "BONEMASK_LOD_LO") == 0)
						{
							m_iCaptureFilterNameIndex = i;

							break;
						}
					}
				}
			}
		}

		m_pBank->AddCombo("Filter", &m_iCaptureFilterNameIndex, m_iCaptureFilterNameCount, m_szCaptureFilterNames);
		m_pBank->AddToggle("Relative To Parent", &m_bRelativeToParent);
		m_pBank->AddToggle("Apply Captured DoFs", &m_bApplyCapturedDofs);
		memset(m_szMotionTreeCapture, '\0', m_uMotionTreeCaptureLength);
		strcpy(m_szMotionTreeCapture, "MoveDump");
		m_pBank->AddText("Motion tree capture filename:", m_szMotionTreeCapture, m_uMotionTreeCaptureLength);
		m_pBank->AddButton("Load motion tree capture", LoadMotionTreeCapture);
		m_pBank->AddButton("Save motion tree capture", SaveMotionTreeCapture);
		m_pBank->AddButton("Print current motion tree capture", PrintMotionTreeCapture);
		m_pBank->AddButton("Print all motion tree capture", PrintAllMotionTreeCapture);
		m_pBank->AddSlider("Render motion tree capture frame", &m_uRenderMotionTreeCaptureFrame, 0, GetMotionTreeCaptureCount() - 1, 1);
#if CALLSTACK_ON_BLOCKING_ANIM_PREPHYSICS_UPDATE
		m_pBank->AddButton("Potential Motion Tree Block Callstack Collector", PotentialMotionTreeBlockCallstackCollector);
#endif //CALLSTACK_ON_BLOCKING_ANIM_PREPHYSICS_UPDATE
	m_pBank->PopGroup();

	m_pBank->PushGroup("Entity Capture");
		m_pBank->AddButton("Add Entity to Capture", EntityCaptureAddEntity);
		m_pEntitiesToCaptureGroup = m_pBank->PushGroup("Entities to Capture");
		m_pBank->PopGroup();
		m_pBank->AddToggle("Lock Game Update to 30 fps", &m_EntityCaptureLock30Fps, EntityCaptureToggleLock30Fps);
		m_pBank->AddToggle("Lock Game Update to 60 fps", &m_EntityCaptureLock60Fps, EntityCaptureToggleLock60Fps);
		m_pBank->AddButton("Start capture", EntityCaptureStart);
		m_pBank->AddButton("Stop capture", EntityCaptureStop);
		m_pBank->AddButton("Capture single frame", EntityCaptureSingleFrame);
	m_pBank->PopGroup();

	m_pBank->PushGroup("Camera Capture");

		if(m_iCameraCapture_CameraNameCount == 0)
		{
			const fwBasePool *pBaseCameraPool = camBaseCamera::GetPool();
			if(pBaseCameraPool && pBaseCameraPool->GetSize())
			{
				for(int i = 0; i < pBaseCameraPool->GetSize(); i ++)
				{
					const camBaseCamera *pBaseCamera = static_cast< const camBaseCamera * >(pBaseCameraPool->GetSlot(i));
					if(pBaseCamera && pBaseCamera->GetName())
					{
						int iBaseCameraNameLength = istrlen(pBaseCamera->GetName());
						if(iBaseCameraNameLength > 0)
						{
							m_iCameraCapture_CameraNameCount ++;
						}
					}
				}

				if(m_iCameraCapture_CameraNameCount > 0)
				{
					m_szCameraCapture_CameraNames = rage_new const char *[m_iCameraCapture_CameraNameCount];

					for(int i = 0, j = 0; i < pBaseCameraPool->GetSize(); i ++)
					{
						const camBaseCamera *pBaseCamera = static_cast< const camBaseCamera * >(pBaseCameraPool->GetSlot(i));
						if(pBaseCamera && pBaseCamera->GetName())
						{
							int iBaseCameraNameLength = istrlen(pBaseCamera->GetName());
							if(iBaseCameraNameLength > 0)
							{
								char *szCameraAnimationCaptureName = rage_new char[iBaseCameraNameLength + 1];
								strcpy(szCameraAnimationCaptureName, pBaseCamera->GetName());
								m_szCameraCapture_CameraNames[j] = szCameraAnimationCaptureName;

								j ++;
							}
						}
					}
				}
			}
		}

		m_pBank->AddText("Animation name:", m_szCameraCapture_AnimationName, m_uCameraCapture_AnimationNameLength);
		m_pBank->AddCombo("Camera", &m_iCameraCapture_CameraNameIndex, m_iCameraCapture_CameraNameCount, m_szCameraCapture_CameraNames, CameraCapture_CameraNameIndexChanged);
		m_pBank->AddButton("Start capture", CameraCapture_Start);
		m_pBank->AddButton("Stop capture", CameraCapture_Stop);
		m_pBank->AddButton("Capture single frame", CameraCapture_SingleFrame);
	m_pBank->PopGroup();

	m_pBank->PopGroup();

	m_animSelector.AddWidgets(m_pBank);

	//m_pBank->PushGroup("Previewing"); 
	m_pBank->PushGroup("Preview Anims"); 
		m_pBank->AddButton("Load preview network", LoadPreviewNetwork);
		crVCRWidget *pVcrWidget = (crVCRWidget *)m_pBank->AddWidget( *(rage_new crVCRWidget( "Playback", NULL, crVCRWidget::Animation )) );

		crVCRWidget::ActionFuncType fastForwardCB;
		fastForwardCB.Reset< CAnimViewerVCRHelper, &CAnimViewerVCRHelper::GoToEndAnim >(&animViewerVCRHelper);
		pVcrWidget->SetFastForwardOrGoToEndCB(fastForwardCB);

		crVCRWidget::ActionFuncType stepForwardCB;
		stepForwardCB.Reset< CAnimViewerVCRHelper, &CAnimViewerVCRHelper::StepForwardAnim >(&animViewerVCRHelper);
		pVcrWidget->SetGoToNextOrStepForwardCB(stepForwardCB);

		crVCRWidget::ActionFuncType playForwardsCB;
		playForwardsCB.Reset< CAnimViewerVCRHelper, &CAnimViewerVCRHelper::PlayForwardsAnim >(&animViewerVCRHelper);
		pVcrWidget->SetPlayForwardsCB(playForwardsCB);

		crVCRWidget::ActionFuncType pauseCB;
		pauseCB.Reset< CAnimViewerVCRHelper, &CAnimViewerVCRHelper::PauseAnim >(&animViewerVCRHelper);
		pVcrWidget->SetPauseCB(pauseCB);

		crVCRWidget::ActionFuncType playBackwardsCB;
		playBackwardsCB.Reset< CAnimViewerVCRHelper, &CAnimViewerVCRHelper::PlayBackwardsAnim >(&animViewerVCRHelper);
		pVcrWidget->SetPlayBackwardsCB(playBackwardsCB);

		crVCRWidget::ActionFuncType stepBackwardCB;
		stepBackwardCB.Reset< CAnimViewerVCRHelper, &CAnimViewerVCRHelper::StepBackwardAnim >(&animViewerVCRHelper);
		pVcrWidget->SetGoToPreviousOrStepBackwardCB(stepBackwardCB);

		crVCRWidget::ActionFuncType rewindCB;
		rewindCB.Reset< CAnimViewerVCRHelper, &CAnimViewerVCRHelper::GoToStartAnim >(&animViewerVCRHelper);
		pVcrWidget->SetRewindOrGoToStartCB(rewindCB);

		m_pBank->AddButton("Remove Anim", StopAnim); 
		m_pBank->AddButton("Reload Anim", ReloadAnim);
		m_pBank->AddSlider("Phase", &m_Phase, 0.0f, 1.0f, 0.01f, SetPhase); 
		m_pFrameSlider = m_pBank->AddSlider("Frame", &m_Frame, 0.0f, 100.0f, 1.0f, SetFrame);
		m_pBank->AddSlider("Rate", &m_Rate, -4.0f, 4.0f, 0.01f, SetRate);
		m_pBank->AddButton("Reactivate Ped", RemoveNetwork);
		m_pBank->AddToggle("Additive", &ms_bPreviewNetworkAdditive);
		m_pBank->AddToggle("Disable all IK solvers during preview", &ms_bPreviewNetworkDisableIkSolvers);

		m_pBank->PushGroup("Test Blend"); 
			m_blendAnimSelector.AddWidgets(m_pBank);
			m_pBank->AddButton("Start blend anim", StartBlendAnim);
			m_pBank->AddSlider("BlendAnimPhase", &m_AnimBlendPhase, 0.0f, 1.0f, 0.01f, SetAnimBlendPhase); 
			m_pBank->AddSlider("Blend", &m_BlendPhase, 0.0f, 1.0f, 0.01f, SetBlendPhase); 
		m_pBank->PopGroup(); 
	m_pBank->PopGroup(); 

	m_pBank->PushGroup("Preview as facial");
		m_pBank->AddButton("SetFacialIdleClip", SetFacialIdleClip);
		m_pBank->AddButton("PlayOneShotFacialClip", PlayOneShotFacialClip);
			m_pBank->AddButton("PlayOneShotFacialClipNoBlend", PlayOneShotFacialClipNoBlend);
		m_pBank->AddButton("PlayVisemeClip", PlayVisemeClip);
	m_pBank->PopGroup();

	m_pBank->PushGroup("Preview as anim task");
		m_pBank->PushGroup("Anim task flags");
		// Add a toggle for each bit
		for(int i = 0; i < PARSER_ENUM(eScriptedAnimFlags).m_NumEnums; i++)
		{
			const char* name = parser_eScriptedAnimFlags_Strings[i];
			if (name)
			{
				u32 bitsPerBlock = sizeof(u32) * 8;
				int block = i / bitsPerBlock;
				int bitInBlock = i - (block * bitsPerBlock);
				m_pBank->AddToggle(name, (reinterpret_cast<u32*>(&ms_AnimFlags.BitSet()) + block), (u32)(1 << bitInBlock));
			}
		}
		m_pBank->PopGroup(); 
		m_pBank->PushGroup("Ik control flags");
		// Add a toggle for each bit
		for(int i = 0; i < PARSER_ENUM(eIkControlFlags).m_NumEnums; i++)
		{
			const char* name = parser_eIkControlFlags_Strings[i];
			if (name)
			{
				u32 bitsPerBlock = sizeof(u32) * 8;
				int block = i / bitsPerBlock;
				int bitInBlock = i - (block * bitsPerBlock);
				m_pBank->AddToggle(name, (reinterpret_cast<u32*>(&ms_IkControlFlags.BitSet()) + block), (u32)(1 << bitInBlock));
			}
		}
		m_pBank->PopGroup(); 
		m_pBank->AddSlider("blend in duration", &ms_fPreviewAnimTaskBlendIn, 0.0f, 5.0f, 0.001f);
		m_pBank->AddSlider("blend out duration", &ms_fPreviewAnimTaskBlendOut, 0.0f, 5.0f, 0.001f);
		m_pBank->AddSlider("Start phase", &ms_fPreviewAnimTaskStartPhase, 0.0f, 1.0f, 0.001f);
		ms_taskFilterSelector.AddWidgets(m_pBank, "Select filter");
		m_pBank->PushGroup("Advanced mode");
			m_pBank->AddToggle("Enable", &ms_bPreviewAnimTaskAdvanced);
			m_pBank->AddVector("Origin position", &ms_vPreviewAnimTaskAdvancedPosition, -100000.0f, 100000.0f, 0.001f);
			m_pBank->AddVector("Origin orientation", &ms_vPreviewAnimTaskAdvancedOrientation, 0.0f, 360.0f, 0.001f);
		m_pBank->PopGroup(); // "Advanced mode"
		//m_pBank->AddToggle("Additive", &ms_bPreviewAnimTaskAdditive);
		m_pBank->AddButton("Play as an anim task", StartAnimTask);
		m_pBank->AddButton("Stop anim task", StopAnimTask);
	m_pBank->PopGroup();	

	m_pBank->PushGroup("Preview object additive anim");
		m_additiveBaseSelector.AddWidgets(m_pBank);
		m_pBank->AddSlider("Additive base rate", &m_fAdditiveBaseRate, 0.0f, 1.0f, 0.01f);
		m_pBank->AddSlider("Additive base phase", &m_fAdditiveBasePhase, 0.0f, 1.0f, 0.01f);
		m_pBank->AddToggle("Additive base looping", &m_bAdditiveBaseLooping);
		m_additiveOverlaySelector.AddWidgets(m_pBank);
		m_pBank->AddSlider("Additive overlay rate", &m_fAdditiveOverlayRate, 0.0f, 1.0f, 0.01f);
		m_pBank->AddSlider("Additive overlay phase", &m_fAdditiveOverlayPhase, 0.0f, 1.0f, 0.01f);
		m_pBank->AddToggle("Additive overlay looping", &m_bAdditiveOverlayLooping);
		m_pBank->AddButton("Play object additive anim", StartObjectAdditiveAnim);
		m_pBank->AddButton("Stop object additive anim", StopObjectAdditiveAnim);
	m_pBank->PopGroup();

	m_pBank->PushGroup("Preview Paired Anims"); 
		m_pBank->AddButton("Position Focus Ped Relative To Focus Vehicle", PositionPedRelativeToVehicleSeatCB);
		m_pBank->AddToggle("Use anim offset from anim to reach seat target", &ms_bComputeOffsetFromAnim);
		m_pBank->AddSlider("Vehicle seat", &ms_iSeat, 0, 10, 1);
		m_pBank->AddButton("Position Second Focus Ped Relative To First", PositionPedsCB);
		m_pBank->AddButton("Clear Peds", ResetPedsCB);
		m_pBank->AddSlider("Extra Z Offset", &ms_fExtraZOffsetForBothPeds, -5.0f, 5.0f, 0.1f);
		m_pBank->AddSlider("X Component Relative Offset", &ms_fXRelativeOffset, -10.0f, 10.0f, 0.01f);
		m_pBank->AddSlider("Y Component Relative Offset", &ms_fYRelativeOffset, -10.0f, 10.0f, 0.01f);
		m_pBank->AddSlider("Z Component Relative Offset", &ms_fZRelativeOffset, -10.0f, 10.0f, 0.01f);
		m_pBank->AddSlider("Relative Heading", &ms_fRelativeHeading, -PI, PI, 0.01f);
		m_pBank->AddButton("Add Anim To First Ped", AddAnimToFirstPedsListCB);
		m_pBank->AddButton("Add Anim To Second Ped", AddAnimToSecondPedsListCB);
		m_pBank->AddButton("Clear First Peds Anim List", ClearFirstPedsAnimsCB);
		m_pBank->AddButton("Clear Second Peds Anim List", ClearSecondPedsAnimsCB);
		m_pBank->AddButton("Play anims on focus peds", PlayAnimsCB);
	m_pBank->PopGroup(); 

	m_pBank->PushGroup("Preview Camera");
	m_pBank->AddToggle("Use Preview Camera", &ms_bUsePreviewCamera);
	m_pBank->AddCombo("Preview Camera", &ms_iPreviewCameraIndex, ms_iPreviewCameraCount, ms_szPreviewCameraNames);
	m_pBank->PopGroup();
	//m_pBank->PopGroup(); // "Previewing"

	m_pBank->PushGroup("Information", false, NULL);
	{
		m_pBank->PushGroup("Selected anim information", false, NULL);
		{
			m_pBank->AddButton("Dump Selected Clip", DumpClip);
			m_pBank->AddButton("Dump Selected Anim WRT Entity", DumpAnimWRTSelectedEntity);
			m_pBank->AddButton("Dump Pose", DumpPose);
			m_pBank->AddToggle("Print eulers", &m_bDumpPosePrintEulers, NullCB, "");
			m_pBank->AddToggle("Print only DOFs in clip", &m_bDumpPosePrintOnlyDOFsInClip, NullCB, "");
			m_pBank->AddToggle("Render the mover track", &m_drawMoverTrack, NullCB, "");
			m_pBank->AddToggle("Render mover track at entity", &m_renderMoverTrackAtEntity, NullCB, "");
			m_pBank->AddButton("Save matrix for mover track rendering", SetRenderMoverTrackAtFocusTransform, "");

			m_pBank->AddSlider("", &m_drawMoverTrackSize, 0.0f, 1.0f, 0.001f, NullCB, "");
			m_pBank->AddToggle("Render the skeleton for the selected anim", &m_drawSelectedAnimSkeleton, NullCB, "");
			m_pBank->AddSlider("Render at phase:", &m_drawSelectedAnimSkeletonPhase, 0.f, 1.0f, 0.001f, NullCB, "");
			m_pBank->AddSlider("Render offset:",  &m_drawSelectedAnimSkeletonOffset, -4.f, 4.0f, 0.001f, NullCB, "");
			m_pBank->AddToggle("Render tags", &m_renderTagsForSelectedClip, NullCB, "");
			m_pBank->AddToggle("Render tags graphically", &m_renderTagsVisuallyForSelectedClip, NullCB, "");
			m_pBank->AddSlider("Graphical tags render offset:", &m_renderTagsVisuallyForSelectedClipOffset, 0.05f, 0.65f, 0.05f);
			m_pBank->PushGroup("Dof test");
			m_pBank->AddToggle("Test for dof:", &m_drawSelectedAnimSkeletonDOFTest, NullCB, "");
			m_pBank->AddSlider("Track:", &m_drawSelectedAnimSkeletonDOFTestTrack, 0, 255, 1, NullCB, "");
			m_pBank->AddSlider("Id:",  &m_drawSelectedAnimSkeletonDOFTestId, 0, 65535, 1, NullCB, "");
			static const char* pTypeNames[kFormatTypeNum] = {"Vector", "Quaternion", "float"};
			m_pBank->AddCombo("Type", &m_drawSelectedAnimSkeletonDOFTestType, kFormatTypeNum, pTypeNames);
			m_pBank->PopGroup();
		}
		m_pBank->PopGroup(); // "Selected anim information"

		m_pBank->PushGroup("Selected entity information", false, NULL);
		{
			m_pBank->AddButton("Select nearest animated building to player", FindNearestAnimatedBuildingToPlayer, "");
			m_pBank->AddSlider("Select entity by script index", &m_entityScriptIndex, -1, 100000, 1, SetFocusEntityFromIndex);
			m_pBank->AddToggle("Render animations", &m_bVisualiseMotionTreeSimple, ToggleMotionTreeSimple);
			m_pBank->AddToggle("Dump animations", &m_bDumpMotionTreeSimple);
			m_pBank->AddToggle("Render entity info", &ms_bRenderEntityInfo, NullCB, "");
			m_pBank->AddToggle("Render skin",	&m_renderSkin, ToggleRenderPeds, NULL);
			m_pBank->AddToggle("Render skeleton",  &m_renderSkeleton);
#if FPS_MODE_SUPPORTED
			m_pBank->AddToggle("Render Third Person Skeleton", &m_renderThirdPersonSkeleton);
#endif // FPS_MODE_SUPPORTED
			m_pBank->AddToggle("Render selected entity matrix", &m_drawSelectedEntityMat, NullCB, "");
			m_pBank->AddSlider("", &m_drawSelectedEntityMatSize, 0.0f, 1.0f, 0.001f, NullCB, "");
			m_pBank->AddToggle("Render selected entity translation", &m_drawSelectedEntityMatTranslation, NullCB, "");
			m_pBank->AddToggle("Render selected entity rotation", &m_drawSelectedEntityMatRotation, NullCB, "");
			m_pBank->AddToggle("Render selected entity translation and rotation WRT cutscene", &m_drawSelectedEntityMatTransAndRotWrtCutscene, NullCB, "");
			m_pBank->AddToggle("Render motion blur tag data", &m_drawMotionBlurData, NullCB, "");
		}
		m_pBank->PopGroup(); // Selected entity information

		m_pBank->PushGroup("Bone information", false, NULL);
		{
			//m_pBank->AddSlider("Bone index",	 &m_boneIndex, 0, 100, 1.0f, UpdateBoneIndex);
			if (m_boneNames.GetCount() > 0)
			{
				m_pBoneCombo[0] = m_pBank->AddCombo("Select Bone", &m_boneIndex, m_boneNames.GetCount(), &m_boneNames[0], SelectBone);
			}
			m_pBank->AddToggle("Render selected bone matrix", &m_drawSelectedBoneMat, NullCB, "");
			m_pBank->AddToggle("Render selected parent bone matrix", &m_drawSelectedParentBoneMat, NullCB, "");
			m_pBank->AddSlider("", &m_drawSelectedBoneMatSize, 0.0f, 1.0f, 0.001f, NullCB, "");
			m_pBank->AddToggle("Render selected bone translation", &m_drawSelectedBoneMatTranslation, NullCB, "");
			m_pBank->AddToggle("Render selected bone rotation", &m_drawSelectedBoneMatRotation, NullCB, "");
			m_pBank->AddToggle("Render selected bone trans and rotation wrt cutscene", &m_drawSelectedBoneMatTransAndRotWrtCutscene, NullCB, "");
			m_pBank->AddToggle("Render selected bone scale", &m_drawSelectedBoneMatScale, NullCB, "");
			m_pBank->AddToggle("Render all bone matrices", &m_drawAllBonesMat, NullCB, "");
			m_pBank->AddSlider("", &m_drawAllBonesMatSize, 0.0f, 1.0f, 0.001f, NullCB, "");

			m_pBank->AddToggle("Render first person camera", &m_drawFirstPersonCamera, NullCB, "");
			m_pBank->AddToggle("Render constraint helper DOFs", &m_drawConstraintHelpers, NullCB, "");
			m_pBank->AddToggle("Save bone history", &m_autoAddBoneHistoryEveryFrame, NullCB, "");
			m_pBank->AddToggle("Render bone history", &m_renderBoneHistory, NullCB, "");
			m_pBank->AddButton("Clear bone history", ClearBoneHistory, "Empties the bone history data structure");
			m_pBank->AddButton("Output bone history to file", OutputBoneHistory, "Saves the bone history out to a csv file");

			m_pBank->PushGroup("Bone overrides", false, NULL);
			{
				m_pBank->AddToggle("Apply override relatively", &ms_bApplyOverrideRelatively);
				m_pBank->AddToggle("Override bone rotation", &ms_bOverrideBoneRotation, ToggleBoneRotationOverride, "");
				m_pBank->AddSlider("Rotation", &m_rotation, -TWO_PI, TWO_PI, 0.001f, NullCB);
				m_pBank->AddToggle("Override bone translation", &ms_bOverrideBoneTranslation, ToggleBoneTranslationOverride, "");
				m_pBank->AddSlider("Translation", &m_translation, -5.0f, 5.0f, 0.001f, NullCB);
				m_pBank->AddToggle("Override bone scale", &ms_bOverrideBoneScale, ToggleBoneScaleOverride, "");
				m_pBank->AddSlider("Scale", &m_scale, 0.001f, 20.0f, 0.001f, NullCB);
				m_pBank->AddSlider("Override bone blend", &ms_fOverrideBoneBlend, 0.0f, 1.0f, 0.001f);
			}
			m_pBank->PopGroup(); //"Bone overrides"

			m_pBank->PushGroup("Bone limits", false, NULL);
			{
				m_pBank->AddToggle("Render selected bone joint limits", &m_drawSelectedBoneJointLimits, NullCB, "");
			}
			m_pBank->PopGroup();//"Bone limits"
		}

		m_pBank->PopGroup(); // "Bone information"

		m_pBank->PushGroup("Mover track information", false, NULL);
		{
			m_pBank->AddToggle("Render selected bone translation", &m_drawSelectedEntityMoverTranslation, NullCB, "");
			m_pBank->AddToggle("Render selected bone rotation", &m_drawSelectedEntityMoverRotation, NullCB, "");
		}
		m_pBank->PopGroup();

		m_pBank->PushGroup("Hair information", false, NULL);
		{
			m_pBank->AddToggle("Render hair", &m_drawSelectedEntityHair, NullCB, "");
			m_pBank->AddToggle("Measure hair", &m_drawSelectedEntityHairMeasure, NullCB, "");
			m_pBank->AddSlider("Measure hair height", &m_drawSelectedEntityHairMeasureHeight, 0.0f, 0.5f, 0.001f);
		}
		m_pBank->PopGroup();

		m_pBank->PushGroup("Dof Tracking", false, NULL);
		{
			m_pBank->AddToggle("Update post physics (default post render)", &ms_DofTrackerUpdatePostPhysics, NullCB, "");
			if (m_boneNames.GetCount() > 0)
			{
				m_pBoneCombo[1] = m_pBank->AddCombo("Bone 1", &ms_DofTrackerBoneIndices[0], m_boneNames.GetCount(), &m_boneNames[0]);
				m_pBoneCombo[2] = m_pBank->AddCombo("Bone 2", &ms_DofTrackerBoneIndices[1], m_boneNames.GetCount(), &m_boneNames[0]);
				m_pBoneCombo[3] = m_pBank->AddCombo("Bone 3", &ms_DofTrackerBoneIndices[2], m_boneNames.GetCount(), &m_boneNames[0]);
			}
		}
		m_pBank->PopGroup(); // Dof Tracking
		
		m_pBank->PushGroup("FrameFilter dictionary information", false, NULL);
		{
			m_pBank->AddToggle("Render FrameFilter dictionary debug", &m_renderFrameFilterDictionaryDebug, NullCB, "");
		}
		m_pBank->PopGroup();

		m_pBank->PushGroup("Synchronized scene information", false, NULL);
		{
			m_pBank->AddToggle("Render synchronized scene debug", &m_renderSynchronizedSceneDebug, NullCB, "");
			m_pBank->AddSlider("Horizontal Scroll", &m_renderSynchronizedSceneDebugHorizontalScroll, -500, 500, 1);
			m_pBank->AddSlider("Vertical Scroll", &m_renderSynchronizedSceneDebugVerticalScroll, -500, 500, 1);
		}
		m_pBank->PopGroup();

		m_pBank->PushGroup("Facial variation information", false, NULL);
		{
			m_pBank->AddToggle("Render facial variation debug", &m_renderFacialVariationDebug, NullCB, "");
			m_pBank->AddSlider("Number trackers to display", &m_renderFacialVariationTrackerCount, 2, 50, 1);
		}
		m_pBank->PopGroup();

#if __DEV
#if ENABLE_BLENDSHAPES
		m_pBank->PushGroup("Blendshape information", false, NULL);
		{
			m_pBank->AddButton("Print report", PrintBlendshapeReport, "");
			//if (m_blendShapeNames.GetCount() > 0)
			//{
			//	m_pBlendshapeCombo = m_pBank->AddCombo("Select Blendshape", &m_blendShapeIndex, m_blendShapeNames.GetCount(), &m_blendShapeNames[0], NullCB, "");
			//}
		}
		m_pBank->PopGroup();
#endif // ENABLE_BLENDSHAPES
#endif //__DEV
	}
	m_pBank->PopGroup(); // "Information"


	m_pBank->PushGroup("Debugging", false, NULL);
	{
#if __BANK
		m_pBank->PushGroup("Anim memory usage", false);
		{
			m_pBank->AddToggle("Show clip dictionary list", 							&m_showAnimDicts,		NullCB, "");
			m_pBank->AddToggle("Show memory group summary", 							&m_showAnimMemory,		NullCB, "");
			m_pBank->PushGroup("Filtering", true);
			{
				m_pBank->AddToggle("Include resident dictionaries", 					&m_isResident,			NullCB, "");
				m_pBank->AddToggle("Include streaming dictionaries", 					&m_isStreaming,			NullCB, "");
				m_pBank->AddToggle("Include dictionaries that are loaded", 				&m_isLoaded,			NullCB, "");
				m_pBank->AddToggle("Include dictionaries that are not loaded", 		    &m_isNotLoaded,			NullCB, "");
				m_pBank->AddToggle("Include dictionaries that have references", 		&m_haveReferences,		NullCB, "");
				m_pBank->AddToggle("Include dictionaries that have no references", 		&m_haveNoReferences,	NullCB, "");
				m_pBank->AddToggle("Filter by memory group", 							&m_filterByMemoryGroup,	NullCB, "");
				m_pBank->AddCombo("Memory group to filter by", &ms_iSelectedMemoryGroup, fwClipSetManager::GetMemoryGroupNames().GetCount(), &fwClipSetManager::GetMemoryGroupNames()[0], NullCB);
			}
			m_pBank->PopGroup(); // Filtering

			m_pBank->PushGroup("Filtering columns", true);
			{
				m_pBank->AddToggle("Toggle % MG", &m_showPercentageOfMGColumn, NullCB, "");
				m_pBank->AddToggle("Toggle % budget MG", &m_showPercentageOfMGBudgetColumn, NullCB, "");
				m_pBank->AddToggle("Toggle # clips", &m_showClipCountColumn, NullCB, "");
				m_pBank->AddToggle("Toggle loaded", &m_showLoadedColumn, NullCB, "");
				m_pBank->AddToggle("Toggle policy", &m_showPolicyColumn, NullCB, "");
				m_pBank->AddToggle("Toggle # reference", &m_showReferenceCountColumn, NullCB, "");
				m_pBank->AddToggle("Toggle memory group", &m_showMemoryGroupColumn, NullCB, "");
				m_pBank->AddToggle("Toggle compression", &m_showCompressionColumn, NullCB, "");
			}
			m_pBank->PopGroup(); // Filtering columns

			fwClipSetManager::AddWidgets(m_pBank);

			m_pBank->PushGroup("Sorting", true);
			{
				m_pBank->AddToggle("Sort dictionaries by name", &m_sortByName, datCallback(ToggleSortByName), "Sort the anim dictionaries by name");
				m_pBank->AddToggle("Sort dictionaries by size", &m_sortBySize, datCallback(ToggleSortBySize), "Sort the anim dictionaries by size");	
			}
			m_pBank->PopGroup(); // Sorting

			m_pBank->AddSlider("Horizontally scroll debug output", &m_horizontalScroll, -100, 100, 1);
			m_pBank->AddSlider("Vertically scroll debug output", &m_verticalScroll, -100, 100, 1);

			m_pBank->AddSlider("Scroll dictionary list", &m_dictionaryScroll, 0, 6000, 1);
			//m_pBank->AddButton("Scroll up",       datCallback(ScrollUp));
			//m_pBank->AddButton("Scroll down",     datCallback(ScrollDown));
			m_pBank->AddButton("Print memory usage report", datCallback(PrintMemoryUsageReport));
			m_pBank->AddButton("PrintClipDictionariesReferencedByAnyCipSet", datCallback(PrintClipDictionariesReferencedByAnyCipSet));
			//m_pBank->AddButton("PrintClipDictionariesWithMemoryGroup", datCallback(PrintClipDictionariesWithMemoryGroup));
			m_pBank->PopGroup();

			m_pBank->PushGroup("Anim pool information", false);
			m_pBank->AddToggle("Show anim LOD information",			&m_showAnimLODInfo,											NullCB, "");
			m_pBank->AddToggle("Show anim pool information",		&m_showAnimPoolInfo,										NullCB, "");
			m_pBank->PopGroup();

			m_pBank->PushGroup("Anim logging", false);
			m_pBank->AddButton("Log all anims", datCallback(LogAllAnims));
			m_pBank->AddButton("Log all expressions", datCallback(LogAllExpressions));
			m_pBank->AddToggle("Include slot index and hash key",		&m_bLogAnimDictIndexAndHashKeys,							NullCB, "");
			m_pBank->AddButton("Match hashKeys", datCallback(ReadHashKeyFile));
			m_pBank->PopGroup();

			m_pBank->PushGroup("Anim debug", false);
			m_pBank->AddToggle("Enable pose update",					&fwAnimDirector::sm_DebugEnablePoseUpdate,					NullCB, "");
			m_pBank->AddToggle("Enable motion tree logging",			&fwAnimDirector::sm_DebugEnableMotionTreeLogging,			NullCB, "");
		}
		m_pBank->PopGroup();
#endif // __BANK

		m_pBank->PushGroup("Animated building debug");
		{
			m_pBank->AddSlider("Rate", &ms_AnimatedBuildingRate, 0.0f, 2.0f, 0.01f, datCallback(AnimatedBuildingAnimRateChanged));
			m_pBank->AddButton("Random Rate", datCallback(RandomAnimatedBuildingAnimRate));
			m_pBank->AddSlider("Phase", &ms_AnimatedBuildingPhase, 0.0f, 1.0f, 0.01f, datCallback(AnimatedBuildingAnimPhaseChanged));
			m_pBank->AddButton("Random Phase", datCallback(RandomAnimatedBuildingAnimPhase));
		}
		m_pBank->PopGroup(); /* Animated building debug */

		m_pBank->PushGroup("Batch Clip Processing", false, NULL);
		{
			m_pBank->PushGroup("Disk Based Processes", false);
				m_pBank->AddText("Input Folder", m_BatchProcessInputFolder, RAGE_MAX_PATH);
				m_pBank->AddText("Output Folder", m_BatchProcessOutputFolder, RAGE_MAX_PATH);
				m_pBank->AddToggle("Verbose", &m_bVerbose, NullCB, "");

				m_pBank->PushGroup("Check Data", false);
					m_pBank->AddToggle("Check all clips and anims have a valid duration", &m_abClipProcessEnabled[CP_CHECK_CLIP_ANIM_DURATION]);
					m_pBank->AddToggle("Check all tags have a valid range", &m_abClipProcessEnabled[CP_CHECK_TAGS_RANGE]);
					m_pBank->AddToggle("Check for anims where IK_ROOT XY keys are not zero", &m_abClipProcessEnabled[CP_CHECK_IK_ROOT]);
					m_pBank->AddToggle("Check that each frame of the animation is generating valid translations and rotations (mover only)", &m_abClipProcessEnabled[CP_CHECK_ANIM_DATA_MOVER_ONLY]);
					m_pBank->AddToggle("Check for clips that do not have an Audio tag", &m_abClipProcessEnabled[CP_CHECK_FOR_AUDIO_TAGS]);
					m_pBank->PushGroup("Check constraint helper", false);
						m_pBank->AddToggle("Check constraint helper data", &m_abClipProcessEnabled[CP_CHECK_CONSTRAINT_HELPER]);
						m_pBank->AddToggle("Right hand helper (or left hand helper)", &m_ConstraintHelperValidation.m_bRight);
						m_pBank->AddSlider("Tolerance", &m_ConstraintHelperValidation.m_fTolerance, 0.0f, 1.0f, 0.01f);
					m_pBank->PopGroup();
				m_pBank->PopGroup();

				m_pBank->PushGroup("Add Data (updated anims will be in art:/anim/update_mb)", false);
					m_pBank->AddToggle("Add IK Root Track", &m_abClipProcessEnabled[CP_ADD_TRACK_IK_ROOT]);
					m_pBank->AddToggle("Add IK Head Track", &m_abClipProcessEnabled[CP_ADD_TRACK_IK_HEAD]);
					m_pBank->AddToggle("Add Spine Weight Track", &m_abClipProcessEnabled[CP_ADD_TRACK_SPINE_WEIGHT]);
					m_pBank->AddToggle("Add Root Height Track", &m_abClipProcessEnabled[CP_ADD_TRACK_ROOT_HEIGHT]);
					m_pBank->AddToggle("Add High Heel Control Track", &m_abClipProcessEnabled[CP_ADD_TRACK_HIGH_HEEL]);
					m_pBank->AddToggle("Add First Person Camera Weight Track", &m_abClipProcessEnabled[CP_ADD_TRACK_FIRST_PERSON_CAMERA_WEIGHT]);
					m_pBank->PushGroup("High Heel Control Options", false);
					m_pBank->AddToggle("Default to 1.0 (Otherwise 0.0)", &m_bDefaultHighHeelControlToOne);
					m_pBank->PopGroup();
					m_pBank->AddToggle("Add Look IK Tags", &m_abClipProcessEnabled[CP_ADD_TAGS_LOOK_IK]);
					m_pBank->AddToggle("Convert First Person Data", &m_abClipProcessEnabled[CP_CONVERT_FIRST_PERSON_DATA]);
				m_pBank->PopGroup();

				m_pBank->PushGroup("Update Data", false);
					m_pBank->AddToggle("Update Audio Relevant Weight Threshold", &m_abClipProcessEnabled[CP_UPDATE_AUDIO_RELEVANT_WEIGHT_THRESHOLD]);
				m_pBank->PopGroup();

				m_pBank->PushGroup("Delete Data", false);
					m_pBank->AddToggle("Reset IK_ROOT", &m_abClipProcessEnabled[CP_RESET_IK_ROOT]);
					m_pBank->AddToggle("Delete Arm Roll", &m_abClipProcessEnabled[CP_DELETE_TRACK_ARM_ROLL]);
				m_pBank->PopGroup();

				m_pBank->AddButton("Run", BatchProcessClipsOnDisk);

				m_pBank->PushGroup("Find and Replace", false);
					m_pBank->AddText("Property Name", m_FindAndReplaceData.propertyName, 32, false);
					m_pBank->AddText("Attribute Name", m_FindAndReplaceData.attributeName, 32, false);
					m_pBank->AddText("Find What", m_FindAndReplaceData.findWhat, 32, false);
					m_pBank->AddText("Replace With", m_FindAndReplaceData.replaceWith, 32, false);
					m_pBank->AddButton("Replace All", BatchProcessFindAndReplace);
				m_pBank->PopGroup();
			m_pBank->PopGroup();

			m_pBank->PushGroup("Memory Based Processes", false);
				m_pBank->AddButton("Run", BatchProcessClipsInMemory);
			m_pBank->PopGroup();
		}
		m_pBank->PopGroup(); // "Debug skeleton array"

		m_pBank->PushGroup("Debug skeleton array", false, NULL);
		{
			m_pBank->AddButton("Add a debug skeleton", AddDebugSkeleton, "");
			m_pBank->AddToggle("Render the debug skeleton array", &m_drawDebugSkeletonArray, NullCB, "");
			m_pBank->AddToggle("Render using local matrices", &m_drawDebugSkeletonsUsingLocals, NullCB, "");
			m_pBank->AddToggle("Render skeleton names", &m_renderDebugSkeletonNames, NullCB, "");
			m_pBank->AddToggle("Render parent matrices", &m_renderDebugSkeletonParentMatrices, NullCB, "");
			m_pBank->AddToggle("Auto add a debug skeleton every frame (in CAnimViewer::Process)", &m_autoAddSkeletonEveryFrame, NullCB, "");
			
			//TODO - store this group and rebuild it when the debug skeleton array is resized
			/*			m_pBank->PushGroup("Enable / hide skeletons", false, NULL);
			{
			for (int i=0; i<MAX_DEBUG_SKELETON ; ++i)
			{
			char widgetLabel[30];
			sprintf(widgetLabel, "Skeleton Index: %d", i);

			m_pBank->AddText(widgetLabel, &m_skeletonDescriptionArray[i*MAX_DEBUG_SKELETON_DESCRIPTION], MAX_DEBUG_SKELETON_DESCRIPTION, false);
			m_pBank->AddToggle("",&m_skeletonRenderArray[i], NullCB, "");

			}					
			} 
			m_pBank->PopGroup(); // "Enable / hide skeletons
			*/
		}
		m_pBank->PopGroup(); // "Debug skeleton array"

#if ENABLE_BLENDSHAPES
		m_pBank->PushGroup("Blendshapes debug", false, NULL);
		{
			m_pBank->AddButton("Print report", PrintBlendshapeReport, "");
			m_pBank->AddToggle("Reset weights", &crCreatureComponentBlendShapes::ms_bReset, NullCB, "");
			m_pBank->AddButton("Log blendshapes after pose", LogBlendshapesAfterPose, "");
			m_pBank->AddButton("Log blendshapes after finalise", LogBlendshapesAfterFinalize, "");
			m_pBank->AddToggle("Force all weights", &CCustomShaderEffectPed::ms_bBlendshapeForceAll, NullCB, "");
			m_pBank->AddToggle("Force specific weight", &CCustomShaderEffectPed::ms_bBlendshapeForceSpecific, NullCB, "");
			m_pBank->AddSlider("Weight index", &CCustomShaderEffectPed::ms_nBlendshapeForceSpecificIdx, 0, 100, 1, NullCB, "");
			m_pBank->AddSlider("Weight value", &CCustomShaderEffectPed::ms_fBlendshapeForceSpecificBlend, 0.0f, 100.0f, 0.1f, NullCB, "");		}
		m_pBank->PopGroup();
#endif // ENABLE_BLENDSHAPES

		m_pBank->PushGroup("Shader Variable debug", false, NULL);
		{
			//m_pBank->AddButton("Print report", PrintBlendshapeReport, "");
			//m_pBank->AddToggle("Reset weights", &crCreatureComponentBlendShapes::ms_bReset, NullCB, "");

			m_pBank->AddButton("Log shader var after pose", LogShaderVarAfterPose, "");
			m_pBank->AddButton("Log shader var after draw update", LogShaderVarAfterDraw, "");
		}
		m_pBank->PopGroup();

		m_pBank->PushGroup("Posematch NMBS debug", false, NULL);
		{
#if __DEV
			m_pBank->AddButton("Dump posematcher", DumpPosematcher, "");
			m_pBank->AddToggle("Render the generated point cloud for the selected anim", &m_renderPointCloud, NullCB, "");
			m_pBank->AddToggle("Render at target entity skeleton", &m_renderPointCloudAtSkelParent, NullCB, "");
			m_pBank->AddToggle("Render last attempted pose match", &m_renderLastMatchedPose, NullCB, "");
			m_pBank->AddToggle("Force ped matrix upright when ragdoll settles", &CTaskDyingDead::ms_bForceFixMatrixOnDeadRagdollFrame, NullCB, "");
			m_pBank->AddSlider("Point size", &m_pointCloudBoneSize, 0.01f, 0.2f, 0.01f);
			m_pBank->AddButton("Dump Natural Motion Blendout Sets", DumpNMBS, "");
#endif
#if __ASSERT
			m_pBank->AddToggle("Spew tasks and events on getup selection:", &CTaskGetUp::ms_SpewRagdollTaskInfoOnGetUpSelection, NullCB, "Spews task and event info for the ped to the tty after a getup task is selected.");
#endif //__ASSERT
			m_pBank->AddToggle("Force blend out set:", &m_bForceBlendoutSet, NullCB, "Forces peds to use the blend out set specified below when blending from nm");
			m_pBank->AddText("Forced set", &m_ForcedBlendOutSet, false);

			m_BlendOutDataGroup = m_pBank->AddGroup("Blend Out Data", false, "Data for the last 128 blend outs for the currently selected ped");
			m_BlendOutDataGroup->AddToggle("Enable Blend Out Data Recording", &m_BlendOutDataEnabled, NullCB, "Enable recording of blend out data");
			m_BlendOutDataGroup->AddButton("Clear Blend Out Data", datCallback(CFA(CAnimViewer::ClearBlendOutData)));
			m_BlendOutDataGroup->AddButton("Reactivate Selected Ped", datCallback(CFA1(CAnimViewer::ReactivateSelectedPed)));
			m_BlendOutDataGroup->AddText("Ragdoll Pose Name", m_BlendOutAnimationName, RAGE_MAX_PATH);
			
			for (atSNode<BlendOutData>* blendOutData = m_BlendOutData.GetHead(); blendOutData != NULL; blendOutData = blendOutData->GetNext())
			{
				blendOutData->Data.AddWidgets(m_BlendOutDataGroup);
			}
		}
		m_pBank->PopGroup();
#if __DEV
		m_pBank->PushGroup("ClipSets debug", false, NULL);
		{
			m_pBank->AddButton("Dump ClipSets CSV data", DumpClipSetsCSV, "Dumps ClipSets to a CSV log file (ClipSets.csv) ready to import into Excel.");
			m_pBank->AddButton("Validate ClipSets", fwClipSetManager::ValidateClipSets, "Checks all ClipSets to make sure their ClipItems point to valid clips, dumps invalid ones to a CSV log file (ValidateClipSets.csv) ready to import into Excel.");
		}
		m_pBank->PopGroup();

		m_pBank->PushGroup("Filters debug", false, NULL);
		{
			m_pBank->AddButton("Dump filters to Filters.csv", DumpFiltersCSV, "Dumps Filters to a CSV log file (Filters.csv) ready to import into Excel.");
			m_pBank->AddButton("Make filters in memory from filters.xml", LoadFilters, "Load Filters from filters.xml and save out the .iff files.");
			m_pBank->AddButton("Save filters in memory to filters.xml", SaveFilters, "Save Filters to filters.xml.");
		}
		m_pBank->PopGroup();

#if CRMT_NODE_2_DEF_MAP
		if(PARAM_movedebug.Get())
		{
			m_pBank->PushGroup("Networks debug", false, NULL);
			{
				m_pBank->AddToggle("Enable", &m_bEnableNetworksDebug, NullCB, "");
				m_pBank->AddText("Network Filter", m_networkDebugNetworkFilter, 256, false);
				m_pBank->AddButton("Dump to TTY", DumpNetworkNodePropertyData);
				m_pNetworkNodePropertyData = m_pBank->AddText("Data", m_networkDebugData, m_networkDebugDataLength, true);
			}
			m_pBank->PopGroup();
		}
#endif //CRMT_NODE_2_DEF_MAP

		m_pBank->PushGroup("Gesture tester", false, NULL);
		{
			m_pBank->AddText("Clip", &ms_gestureClipNameHash, false);
			m_pBank->AddSlider("MarkerTime", &ms_gestureData.MarkerTime, 0, 65535, 1); //u32 MarkerTime;
			m_pBank->AddSlider("StartPhase", &ms_gestureData.StartPhase, 0.0f, 1.0f, 0.01f); //u32 StartFrame;
			m_pBank->AddSlider("EndPhase", &ms_gestureData.EndPhase, 0.0f, 1.0f, 0.01f); //u32 EndFrame;
			m_pBank->AddSlider("Rate", &ms_gestureData.Rate, 0.0f, 4.0f, 0.01f); //f32 Rate;
			m_pBank->AddSlider("MaxWeight", &ms_gestureData.MaxWeight, 0.0f, 1.0f, 0.01f); //f32 MaxWeight;
			m_pBank->AddSlider("BlendInTime", &ms_gestureData.BlendInTime, 0.0f, 1.0f, 0.01f); //f32 BlendInTime;
			m_pBank->AddSlider("BlendOutTime", &ms_gestureData.BlendOutTime, 0.0f, 1.0f, 0.01f); //f32 BlendOutTime;
			m_pBank->AddSlider("IsSpeaker", &ms_gestureData.IsSpeaker, 0, 65535, 1); //u32 IsSpeaker;
			m_pBank->AddButton("Blend in gesture", BlendInGesture);
			m_pBank->AddButton("Blend out gestures", BlendOutGestures);
			m_pBank->AddCombo("Select gesture clipset", &m_gestureClipSetIndex, fwClipSetManager::GetClipSetNames().GetCount(), &fwClipSetManager::GetClipSetNames()[0], SelectWeaponClipSet, "Lists the clipsets");
		}
		m_pBank->PopGroup();

		m_pBank->PushGroup("Camera curve viewer", false, NULL);
		{
			m_pBank->AddToggle("Render the selected curve", &ms_bRenderCameraCurve, NullCB, "");
			m_pBank->AddSlider("ms_fRenderCameraCurveP1X", &ms_fRenderCameraCurveP1X, 0.0f, 1.0f, 0.01f);
			m_pBank->AddSlider("ms_fRenderCameraCurveP1Y", &ms_fRenderCameraCurveP1Y, 0.0f, 1.0f, 0.01f);
			m_pBank->AddSlider("ms_fRenderCameraCurveP2X", &ms_fRenderCameraCurveP2X, 0.0f, 1.0f, 0.01f);
			m_pBank->AddSlider("ms_fRenderCameraCurveP2Y", &ms_fRenderCameraCurveP2Y, 0.0f, 1.0f, 0.01f);
			m_pBank->AddSlider("ms_fRenderCameraCurveP2Y", &ms_fRenderCameraCurveP2Y, 0.0f, 1.0f, 0.01f);
			m_pBank->AddSlider("ms_fRenderCameraCurveScale", &ms_fRenderCameraCurveScale, 0.0f, 1.0f, 0.01f);
			m_pBank->AddSlider("ms_fRenderCameraCurveOffsetX", &ms_fRenderCameraCurveOffsetX, -1.0f, 1.0f, 0.01f);
			m_pBank->AddSlider("ms_fRenderCameraCurveOffsetY", &ms_fRenderCameraCurveOffsetY, -1.0f, 1.0f, 0.01f);
			m_pBank->AddCombo("CurveType", &ms_iSelectedCurveType, eCurveType_NUM_ENUMS, parser_eCurveType_Strings);
			m_pBank->AddToggle("ShouldBlendIn", &ms_bShouldBlendIn, NullCB, "");
		}
		m_pBank->PopGroup();
#endif //__DEV

		m_pBank->PushGroup("Create object", false, NULL);
		{
			m_objectSelector.AddWidgets(m_pBank, "Object name");
			m_pBank->AddButton("Spawn object", SpawnSelectedObject, "Creates an instance of the object selected in the combo box");
			m_pBank->AddButton("Select last spawned object", SelectLastSpawnedObject, "Creates an instance of the object selected in the combo box");
		}
		m_pBank->PopGroup(); // "Create object"
	}

	m_pBank->PushGroup("Hash converter", false, NULL);
	{
		m_pBank->AddText("HashString", &ms_hashString, false,  UpdateValueFromString);
		m_pBank->AddText("HashValue", ms_hashValue, 16, false, UpdateStringFromValue);
	}
	m_pBank->PopGroup();

	m_pBank->PopGroup(); // "Debugging"


	m_pBank->PushGroup("Expression Debugging", false, NULL);
	{
		m_pExpressionParentGroup = m_pBank->PushGroup("Expressions", false, NULL);
#if DEBUG_EXPRESSIONS
		if(PARAM_debugexpr.Get())
		{
			m_pBank->AddButton("Update expression debug widgets", AddExpressionDebugWidgets, "");
		}
		else
		{
			m_pBank->AddTitle("You need to run with -debugExpr to debug expressions");
		}
#endif // DEBUG_EXPRESSIONS
		m_pBank->PopGroup();

		m_pLinearPhysicalExpressionGroup = m_pBank->PushGroup("Physical Expressions", false, NULL);
		m_pSpringBoneCombo = m_pBank->AddCombo("Select spring", &m_springIndex, 0, NULL, SelectSpring);

		PopulateSpringWidgets();
		m_pBank->PopGroup(); // "Physical Expressions"
	}
	m_pBank->PopGroup(); // Tuning

	m_previewNetworkGroup = m_pBank->PushGroup("Create MoVE network", false, NULL);
	{
		m_pBank->AddText("Directory:", const_cast<char*>(m_moveFileSelector.GetDirectoryPath()), u32(strlen(m_moveFileSelector.GetDirectoryPath())+1),true);
		m_moveFileSelector.AddWidgets(m_pBank, "Select network file");
		//m_pBank->AddCombo("Network precedence:", &m_moveNetworkPrecedence, (int)CMovePed::kNumPrecedence, &m_precedenceNames[0], NullCB);
		fwClipSetManager::AddClipSetWidgets(m_pBank, &m_moveNetworkClipSet, UpdateMoveClipSet);
		strcpy(ms_clipSetVariableName, CLIP_SET_VAR_ID_DEFAULT.GetCStr());
		m_pBank->AddText("Anim set variable", ms_clipSetVariableName, 64);
		m_pBank->AddButton("Create anim set variable", CreateClipSetVariable);
		m_pBank->AddButton("Delete anim set variable", DeleteClipSetVariable);
		m_clipSetVariableGroup = m_pBank->PushGroup("Anim set variables", true);
		{
			//This gets filled in later - see UpdateClipSetVariables()
		}
		m_pBank->PopGroup(); // Anim set variables

		m_pBank->PushGroup("Test task options", true);
		{
			m_pBank->AddSlider("Blend duration", &CTaskPreviewMoveNetwork::sm_blendDuration, 0.0f, 1.0f, 0.0001f);
			m_pBank->PushGroup("Task slot flags", false);
			{
				m_pBank->AddToggle("Ignore mover blend", &CTaskPreviewMoveNetwork::sm_Task_IgnoreMoverBlend, NullCB, "");
				m_pBank->AddToggle("Ignore mover blend rotation", &CTaskPreviewMoveNetwork::sm_Task_IgnoreMoverBlendRotation, NullCB, "");
				m_pBank->AddToggle("Tag sync (continuous)", &CTaskPreviewMoveNetwork::sm_Task_TagSyncContinuous, NullCB, "");
				m_pBank->AddToggle("Tag sync during blend in / out", &CTaskPreviewMoveNetwork::sm_Task_TagSyncTransition, NullCB, "");
			}
			m_pBank->PopGroup(); // Task slot flags
			m_pBank->AddToggle("Test as secondary task",  &CTaskPreviewMoveNetwork::sm_secondary,	NullCB, "");
			m_pBank->AddToggle("Block movement",  &CTaskPreviewMoveNetwork::sm_BlockMovementTasks,	NullCB, "");
			m_pBank->PushGroup("Secondary slot flags", false);
			{
				m_pBank->AddToggle("Tag sync (continuous)", &CTaskPreviewMoveNetwork::sm_Secondary_TagSyncContinuous, NullCB, "");
				m_pBank->AddToggle("Tag sync during blend in / out", &CTaskPreviewMoveNetwork::sm_Secondary_TagSyncTransition, NullCB, "");
			}
			m_pBank->PopGroup(); // Secondary slot flags
			CTaskPreviewMoveNetwork::sm_filterSelector.AddWidgets(m_pBank, "Filter");
		}
		m_pBank->PopGroup(); // Anim set variables

		m_pBank->AddButton("Load selected network", LoadNetworkButton);
		m_pBank->AddButton("Remove current preview network", RemoveCurrentPreviewNetwork);
		//init parameter widgets here
		m_pBank->PushGroup("Hookup parameter");
		{
			m_pBank->AddText("Parameter name",&m_newParamName[0], 64);
			m_pBank->AddCombo( "Type", &m_newParamType, (int)CMoveParameterWidget::kParameterWidgetNum , CMoveParameterWidget::GetWidgetTypeNames(), CAnimViewer::RequestParamWidgetOptionsRebuild );
			m_widgetOptionsGroup = m_pBank->PushGroup("widget options", false);
			{
				//This gets filled in later - see RebuildParamWidgetOptions()
			}
			m_pBank->PopGroup(); // widget options
			m_pBank->AddButton("Create widgets", AddParamWidget );
			m_pBank->AddButton("Save widgets", SaveParamWidgets );
			m_pBank->AddButton("Clear all widgets", ClearParamWidgets );
		}
		m_pBank->PopGroup(); // hookup parameter
		m_pBank->PushGroup("Force state");
		{
			m_pBank->AddText("Parent", &m_stateMachine[0], sizeof(m_stateMachine));
			m_pBank->AddText("Force to",  &m_forceToState[0], sizeof(m_forceToState));
			m_pBank->AddButton("Force Select State", ForceSelectState);
		}
		m_pBank->PopGroup(); // Force state
		m_pBank->AddButton("Reload all networks", ReloadAllNetworks);

		m_pBank->PushGroup("Test task");// test task
		{
			m_pBank->AddButton("Run test task on focus ped", RunMoVETestTask);
			m_pBank->AddButton("Clear test task on focus ped", StopMoVETestTask);
			m_pBank->AddSlider("Test task reference phase", &CTaskTestMoveNetwork::ms_ReferencePhase, 0.0f, 1.0f, 0.0001f);
		}
		m_pBank->PopGroup(); // test task
	}
	m_pBank->PopGroup();

	m_pBank->PushGroup("Options", false, NULL);
	{
		m_pBank->AddToggle("Render rotations as euler or matrix", &m_eulers, NullCB, "");
		m_pBank->AddToggle("Render angles in degrees or eulers", &m_degrees, NullCB, "");
		m_pBank->AddButton("Toggle Ped Silhouette Mode", CRenderPhaseDebugOverlayInterface::TogglePedSilhouetteMode);
		m_pBank->AddToggle("Toggle Lock Position",  &m_bLockPosition, ToggleLockPosition, NULL);
		m_pBank->AddVector("Set the lock position", reinterpret_cast<Vector3*>(&m_startPosition), bkVector::FLOAT_MIN_VALUE, bkVector::FLOAT_MAX_VALUE, 0.001f);
		m_pBank->AddToggle("Toggle Lock Rotation", &m_bLockRotation, NullCB, NULL);
		m_pBank->AddToggle("Toggle Reset Matrix", &m_bResetMatrix, NullCB, NULL);
		m_pBank->AddToggle("Toggle Gravity", &m_extractZ, ToggleExtractZ, NULL);
		m_pBank->AddButton("Clear primary task", ClearPrimaryTaskOnSelectedPed );
		m_pBank->AddButton("Clear secondary task", ClearSecondaryTaskOnSelectedPed );
		m_pBank->AddButton("Clear motion task network", ClearMotionTaskOnSelectedPed );
		m_pBank->AddToggle("Toggle Task", &m_unInterruptableTask, ToggleUninterruptableTask, NULL);
		// Add a combo box containing the names of all of the available models
		m_pBank->AddCombo("Select Model", &m_modelIndex, m_modelNames.GetCount(), &m_modelNames[0], SelectPed,
			"Lists the models");
		m_pBank->AddCombo("Select motion clipset", &m_motionClipSetIndex, fwClipSetManager::GetClipSetNames().GetCount(), &fwClipSetManager::GetClipSetNames()[0], SelectMotionClipSet,
			"Lists the clipsets");
		m_pBank->AddCombo("Select weapon clipset", &m_weaponClipSetIndex, fwClipSetManager::GetClipSetNames().GetCount(), &fwClipSetManager::GetClipSetNames()[0], SelectWeaponClipSet,
			"Lists the clipsets");
		m_pBank->AddButton("Clear weapon clipset", ClearWeaponClipSet );
		m_pBank->AddCombo("Select facial clipset", &m_facialClipSetIndex, fwClipSetManager::GetClipSetNames().GetCount(), &fwClipSetManager::GetClipSetNames()[0], SelectFacialClipSet,
			"Lists the clipsets");
		m_pBank->AddToggle("Enable facial", &m_bEnableFacial, ToggleEnableFacial);
	}
	m_pBank->PopGroup();

	RebuildParamWidgetOptions();

	//this is just a waste of memory now
	//InitPreviewWidgets();
}

void CAnimViewer::DeactivateBank()
{
	ms_taskFilterSelector.RemoveWidgets();

	// Enable ambient idles.
	CTaskAmbientClips::BLOCK_ALL_AMBIENT_IDLES = false;

	if(m_iCaptureFilterNameCount > 0)
	{
		for(int i = 0; i < m_iCaptureFilterNameCount; i ++)
		{
			delete[] m_szCaptureFilterNames[i]; m_szCaptureFilterNames[i] = NULL;
		}

		delete[] m_szCaptureFilterNames; m_szCaptureFilterNames = NULL;

		m_iCaptureFilterNameCount = 0;
	}

	m_iCaptureFilterNameIndex = 0;

	if(m_iCameraCapture_CameraNameCount > 0)
	{
		for(int i = 0; i < m_iCameraCapture_CameraNameCount; i ++)
		{
			delete[] m_szCameraCapture_CameraNames[i]; m_szCameraCapture_CameraNames[i] = NULL;
		}

		delete[] m_szCameraCapture_CameraNames; m_szCameraCapture_CameraNames = NULL;

		m_iCameraCapture_CameraNameCount = 0;
	}

	m_iCameraCapture_CameraIndex = 0;
	m_iCameraCapture_CameraNameIndex = 0;

	m_animSelector.RemoveWidgets(m_pBank);		// shut down the anim selector
	m_animSelector.Shutdown();
	m_blendAnimSelector.RemoveWidgets(m_pBank);		// shut down the blend anim selector
	m_blendAnimSelector.Shutdown();	
	m_additiveBaseSelector.RemoveWidgets(m_pBank);		// shut down the additive base selector
	m_additiveBaseSelector.Shutdown();
	m_additiveOverlaySelector.RemoveWidgets(m_pBank);		// shut down the additive overlay selector
	m_additiveOverlaySelector.Shutdown();

	m_MoveNetworkHelper.ReleaseNetworkPlayer();
	m_AnimHash.Null();
	m_BlendAnimHash.Null();
	m_objectSelector.RemoveWidgets();	// shut down the object selector
	m_moveFileSelector.RemoveWidgets();	// shut down the move network selector

	ClearBlendOutData();

	ClearAllWidgetPointers();			// we're about to delete all the widgets, so we need to remove any dangling pointers, etc
	m_pBank->RemoveAllMainWidgets();	// remove all the widgets we added in ActivateBank
	
	m_active = false;
#if __DEV
	fwAnimDirector::SetForceLoadAnims(false);
#endif // __DEV
}

void CAnimViewer::BlendInGesture()
{
	if (m_pDynamicEntity && m_pDynamicEntity->GetIsTypePed())
	{
		ms_gestureData.ClipNameHash = ms_gestureClipNameHash;

		CPed* pPed = static_cast<CPed*>(m_pDynamicEntity.Get()); 
		pPed->ProcessGestureHashKey(&ms_gestureData, true, true);
	}
}

void CAnimViewer::BlendOutGestures()
{
	if (m_pDynamicEntity && m_pDynamicEntity->GetIsTypePed())
	{
		CPed* pPed = static_cast<CPed*>(m_pDynamicEntity.Get()); 
		pPed->BlendOutGestures(false);
	}
}

void CAnimViewer::SelectGestureClipSet()
{
	if (m_pDynamicEntity && m_pDynamicEntity->GetIsTypePed())
	{
		CPed* pPed = static_cast<CPed*>(m_pDynamicEntity.Get());
		if (pPed)
		{
			fwMvClipSetId selectedSet(fwClipSetManager::GetClipSetNames()[m_gestureClipSetIndex]);
			fwClipSetManager::StreamIn_DEPRECATED(selectedSet);
			pPed->SetGestureClipSet(selectedSet);
		}
	}
}

void CAnimViewer::UpdateValueFromString()
{
	sprintf(ms_hashValue, "%u", ms_hashString.GetHash());
}
void CAnimViewer::UpdateStringFromValue()
{
	u32 val;

	sscanf(ms_hashValue, "%u", &val);

	ms_hashString.SetHash(val);
}

void CAnimViewer::ClearAllWidgetPointers()
{
	m_pLinearPhysicalExpressionGroup = NULL;
	m_pSpringBoneCombo = NULL;
	m_pSpringGravity = NULL;
	m_pSpringDirection = NULL;
	memset(&m_linearSpringDofs, 0, sizeof(SpringDof));
	memset(&m_angularSpringDofs, 0, sizeof(SpringDof));

	// Pointer to the bone name combo
	for (s32 i=0; i<NUM_BONE_COMBOS; i++)
	{
		m_pBoneCombo[i] = NULL;
	}

#if ENABLE_BLENDSHAPES
	// Pointer to the blendshape combo
	m_pBlendshapeCombo = NULL;
	m_pBlendshapeBank = NULL;
#endif // ENABLE_BLENDSHAPES

	m_previewNetworkGroup = NULL;

	m_pExpressionParentGroup = NULL;
	m_pExpressionWidgetGroup = NULL;

	m_BlendOutDataGroup = NULL;
}

void CAnimViewer::ShutdownLevelWidgets()
{
	if (m_pBank)
	{
		m_pBank->Shutdown();

		m_pBank = NULL;
	}

	CAnimClipEditor::Shutdown();

	fwClipSetManager::ShutdownWidgets();

}

void ToggleDrawDynamicCollision()
{
#if __PFDRAW
	PFDGROUP_Physics.SetEnabled(CPhysics::ms_bDrawDynamicCollision);
	PFD_Active.SetEnabled(CPhysics::ms_bDrawDynamicCollision);
	PFD_Inactive.SetEnabled(CPhysics::ms_bDrawDynamicCollision);
	PFD_Fixed.SetEnabled(!CPhysics::ms_bDrawDynamicCollision);
#endif // __PFDRAW
}

#endif // __BANK

void CAnimViewer::LoadPreviewNetwork()
{
	// Lazily create the skeleton if it doesn't exist
	if (!m_pDynamicEntity->GetSkeleton())
		m_pDynamicEntity->CreateSkeleton();

	if(m_pDynamicEntity->GetIsTypeObject())
	{
		CObject * pObject = static_cast<CObject *>(m_pDynamicEntity.Get());
		pObject->InitAnimLazy();
	}

	// Lazily create the animation director
	if (!m_pDynamicEntity->GetAnimDirector())
		m_pDynamicEntity->CreateAnimDirector(*m_pDynamicEntity->GetDrawable());

	if(m_pDynamicEntity && m_pDynamicEntity->GetAnimDirector())
	{
		m_MoveNetworkHelper.ForceLoadNetworkDef(CClipNetworkMoveInfo::ms_NetworkGenericClip); 
			
		m_MoveNetworkHelper.CreateNetworkPlayer(m_pDynamicEntity, CClipNetworkMoveInfo::ms_NetworkGenericClip); 
		
		if(!m_pDynamicEntity->GetIsOnSceneUpdate()){
			m_pDynamicEntity->AddToSceneUpdate();
		}

		if(m_pDynamicEntity->GetIsTypePed())
		{
			CPed* pPed = static_cast<CPed*>(m_pDynamicEntity.Get()); 
		
			pPed->GetPedIntelligence()->AddTaskAtPriority(rage_new CTaskDoNothing(-1, 0), PED_TASK_PRIORITY_PRIMARY , true);	

			if (ms_bPreviewNetworkAdditive)
			{
				pPed->GetMovePed().SetAdditiveNetwork(m_MoveNetworkHelper.GetNetworkPlayer(), 0.0f); 
			}
			else
			{
				pPed->GetMovePed().SetTaskNetwork(m_MoveNetworkHelper.GetNetworkPlayer(), 0.0f); 
			}

			if (ms_bPreviewNetworkDisableIkSolvers)
			{
				pPed->GetIkManager().ms_DisableAllSolvers = true;
			}
		}
		else if(m_pDynamicEntity->GetIsTypeObject())
		{
			CObject * pObject = static_cast<CObject *>(m_pDynamicEntity.Get());
			if (fragInst * pEntityFragInst = m_pDynamicEntity->GetFragInst())
			{
				// make sure the object has a fragment cache entry
				fragCacheEntry* pEntityCacheEntry = pEntityFragInst->GetCacheEntry();
				if (Verifyf(pEntityCacheEntry, "Object entity has a null cache entry"))
					pEntityCacheEntry->LazyArticulatedInit();
			}
			pObject->SetTask(NULL);
		}
		else if(m_pDynamicEntity->GetIsTypeVehicle())
		{
			((CVehicle &) *m_pDynamicEntity).GetMoveVehicle().SetNetwork(m_MoveNetworkHelper.GetNetworkPlayer(), 0.0f); 
		}
	}

	// Reset the chosen animation
	m_AnimHash="";
	m_BlendAnimHash="";
}

void CAnimViewer::RemoveNetwork()
{
	if(m_pDynamicEntity && m_pDynamicEntity->GetIsTypePed() && m_pDynamicEntity->GetAnimDirector())
	{
		CPed* pPed = static_cast<CPed*>(m_pDynamicEntity.Get()); 

		CTask* pTask = pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_DO_NOTHING); 
		
		if(pTask)
		{
			pPed->GetPedIntelligence()->AddTaskAtPriority(rage_new CTaskDoNothing(10), PED_TASK_PRIORITY_PRIMARY , true);	
		}
		
		if(m_MoveNetworkHelper.IsNetworkAttached())
		{
			if (m_pDynamicEntity->GetIsTypePed())
			{

				((CPed &) *m_pDynamicEntity).GetMovePed().SetTaskNetwork(NULL, 0.0f);
			}
		}

		if (ms_bPreviewNetworkDisableIkSolvers)
		{
			pPed->GetIkManager().ms_DisableAllSolvers = false;
		}
	}
}

void CAnimViewer::SetFacialIdleClip()
{
	if(m_pDynamicEntity && m_pDynamicEntity->GetIsTypePed() && m_pDynamicEntity->GetAnimDirector())
	{
		CPed *pPed = static_cast< CPed * >(m_pDynamicEntity.Get());
		if(pPed)
		{
			crClip *pClip = m_animSelector.GetSelectedClip();
			if(pClip)
			{
				pPed->GetMovePed().SetFacialIdleClip(pClip, NORMAL_BLEND_DURATION);
			}
		}
	}
}

void CAnimViewer::PlayOneShotFacialClip()
{
	if(m_pDynamicEntity && m_pDynamicEntity->GetIsTypePed() && m_pDynamicEntity->GetAnimDirector())
	{
		CPed *pPed = static_cast< CPed * >(m_pDynamicEntity.Get());
		if(pPed)
		{
			crClip *pClip = m_animSelector.GetSelectedClip();
			if(pClip)
			{
				pPed->GetMovePed().PlayOneShotFacialClip(pClip, NORMAL_BLEND_DURATION);
			}
		}
	}
}

void CAnimViewer::PlayOneShotFacialClipNoBlend()
{

	if(m_pDynamicEntity && m_pDynamicEntity->GetIsTypePed() && m_pDynamicEntity->GetAnimDirector())
	{
		CPed *pPed = static_cast< CPed * >(m_pDynamicEntity.Get());
		if(pPed)
		{
			crClip *pClip = m_animSelector.GetSelectedClip();
			if(pClip)
			{
				pPed->GetMovePed().PlayOneShotFacialClip(pClip, INSTANT_BLEND_DURATION);
			}
		}
	}
}


void CAnimViewer::PlayVisemeClip()
{
	if(m_pDynamicEntity && m_pDynamicEntity->GetIsTypePed() && m_pDynamicEntity->GetAnimDirector())
	{
		CPed *pPed = static_cast< CPed * >(m_pDynamicEntity.Get());
		if(pPed)
		{
			crClip *pClip = m_animSelector.GetSelectedClip();
			if(pClip)
			{
				pPed->GetMovePed().PlayVisemeClip(pClip, NORMAL_BLEND_DURATION);
			}
		}
	}
}

void CAnimViewer::StartAnimTask()
{
	if (m_pDynamicEntity && !m_pDynamicEntity->GetIsTypeAnimatedBuilding())
	{
		CTask* pTask = NULL;
		switch(m_animSelector.GetClipContext())
		{
		case kClipContextNone:
			{
				Assertf(false, "Must select a valid context!");
			} break;
		case kClipContextAbsoluteClipSet:
			{
				Assertf(false, "Viewing clips from a clip set as an anim task is not supported!");
			} break;
		case kClipContextClipDictionary:
			{
				u32 bonemask = (u32)BONEMASK_ALL;

				if (ms_AnimFlags.BitSet().IsSet(AF_UPPERBODY))
					bonemask = (u32)BONEMASK_UPPERONLY;
				
				if (ms_taskFilterSelector.HasSelection())
				{
					bonemask = ms_taskFilterSelector.GetSelectedFilterId();
				}

				if (ms_bPreviewAnimTaskAdvanced)
				{
					Quaternion orientation(Quaternion::sm_I);
					orientation.FromEulers(ms_vPreviewAnimTaskAdvancedOrientation);
					pTask = rage_new CTaskScriptedAnimation( m_animSelector.GetSelectedClipDictionary(), m_animSelector.GetSelectedClipName(), CTaskScriptedAnimation::kPriorityLow, bonemask, ms_fPreviewAnimTaskBlendIn, ms_fPreviewAnimTaskBlendOut, -1 , ms_AnimFlags, ms_fPreviewAnimTaskStartPhase, ms_vPreviewAnimTaskAdvancedPosition, orientation, false, false, ms_IkControlFlags);
				}
				else
				{
					pTask = rage_new CTaskScriptedAnimation( m_animSelector.GetSelectedClipDictionary(), m_animSelector.GetSelectedClipName(), CTaskScriptedAnimation::kPriorityLow, bonemask, ms_fPreviewAnimTaskBlendIn, ms_fPreviewAnimTaskBlendOut, -1 , ms_AnimFlags, ms_fPreviewAnimTaskStartPhase,false, false, ms_IkControlFlags);
				}

			} break;
		case kClipContextLocalFile:
			{
				Assertf(false, "Viewing clips from a local file as an anim task is not supported!");
			} break;
		}

		if (pTask)
		{
		static const char * s_fpsClipSuffixHash = "_FP";

		u32 fpsClipHash = atPartialStringHash(s_fpsClipSuffixHash, atPartialStringHash( m_animSelector.GetSelectedClipName()));
		atHashString finalFpsHash(atFinalizeHash(fpsClipHash));

		static_cast<CTaskScriptedAnimation*>(pTask)->SetFPSClipHash(finalFpsHash);
		}

		if (m_pDynamicEntity->GetType()==ENTITY_TYPE_PED)
		{
			CPed* pPed = static_cast<CPed*>(m_pDynamicEntity.Get()); 
			if (pPed && pTask)
			{
				if (ms_AnimFlags.BitSet().IsSet(AF_SECONDARY))
				{
					pPed->GetPedIntelligence()->AddTaskSecondary( pTask, PED_TASK_SECONDARY_PARTIAL_ANIM );
				}
				else
				{
					pPed->GetPedIntelligence()->AddTaskAtPriority(pTask, PED_TASK_PRIORITY_PRIMARY , true);	
				}
			}
		}
		else if (m_pDynamicEntity->GetType()==ENTITY_TYPE_OBJECT)
		{
			CObject* pObj = static_cast<CObject*>(m_pDynamicEntity.Get()); 
			if (pObj && pTask)
			{
				if (ms_AnimFlags.BitSet().IsSet(AF_SECONDARY))
				{
					pObj->SetTask(pTask, CObjectIntelligence::OBJECT_TASK_TREE_SECONDARY, CObjectIntelligence::OBJECT_TASK_SECONDARY_ANIM);
				}
				else
				{
					pObj->SetTask(pTask, CObjectIntelligence::OBJECT_TASK_TREE_PRIMARY, CObjectIntelligence::OBJECT_TASK_PRIORITY_PRIMARY);
				}
			}
		}
		else if (m_pDynamicEntity->GetType()==ENTITY_TYPE_VEHICLE)
		{
			CVehicle* pVeh = static_cast<CVehicle*>(m_pDynamicEntity.Get()); 
			if (pVeh && pTask)
			{
				pVeh->GetIntelligence()->GetTaskManager()->GetTree(VEHICLE_TASK_TREE_SECONDARY)->SetTask(pTask, VEHICLE_TASK_SECONDARY_ANIM); 
			}
		}
	}
}

void CAnimViewer::StopAnimTask()
{
	if (m_pDynamicEntity)
	{
		if (m_pDynamicEntity->GetType()==ENTITY_TYPE_PED)
		{
			CPed* pPed = static_cast<CPed*>(m_pDynamicEntity.Get()); 
			if (ms_AnimFlags.BitSet().IsSet(AF_SECONDARY))
			{
				pPed->GetPedIntelligence()->ClearTasks(false, true);
			}
			else
			{
				pPed->GetPedIntelligence()->ClearTasks(true, false);
			}
		}
		else if (m_pDynamicEntity->GetType()==ENTITY_TYPE_OBJECT)
		{
			CObject* pObj = static_cast<CObject*>(m_pDynamicEntity.Get()); 
			if (ms_AnimFlags.BitSet().IsSet(AF_SECONDARY))
			{
				pObj->GetObjectIntelligence()->GetTaskManager()->ClearTask(CObjectIntelligence::OBJECT_TASK_TREE_SECONDARY, CObjectIntelligence::OBJECT_TASK_SECONDARY_ANIM);
			}
			else
			{
				pObj->GetObjectIntelligence()->GetTaskManager()->ClearTask(CObjectIntelligence::OBJECT_TASK_TREE_PRIMARY, CObjectIntelligence::OBJECT_TASK_PRIORITY_PRIMARY);
			}
		}
		else if (m_pDynamicEntity->GetType()==ENTITY_TYPE_VEHICLE)
		{
			CVehicle* pVeh = static_cast<CVehicle*>(m_pDynamicEntity.Get());
			pVeh->GetIntelligence()->GetTaskManager()->ClearTask(VEHICLE_TASK_TREE_SECONDARY, VEHICLE_TASK_SECONDARY_ANIM);
			
		}

	}
}

void CAnimViewer::StartObjectAdditiveAnim()
{
	if (m_pDynamicEntity &&
		m_pDynamicEntity->GetIsTypeObject() &&
		m_additiveBaseSelector.HasSelection() &&
		m_fAdditiveBaseRate >= 0.0f && m_fAdditiveBaseRate <= 1.0f &&
		m_fAdditiveBasePhase >= 0.0f && m_fAdditiveBasePhase <= 1.0f &&
		m_additiveOverlaySelector.HasSelection() &&
		m_fAdditiveOverlayRate >= 0.0f && m_fAdditiveOverlayRate <= 1.0f &&
		m_fAdditiveOverlayPhase >= 0.0f && m_fAdditiveOverlayPhase <= 1.0f)
	{
		// Start the new task
		CTaskScriptedAnimation::ScriptInitSlotData priorityLow;

		priorityLow.state.Int = CTaskScriptedAnimation::kStateSingleClip;

		priorityLow.dict0.String = m_additiveBaseSelector.GetSelectedClipDictionary();
		priorityLow.clip0.String = m_additiveBaseSelector.GetSelectedClipName();
		priorityLow.phase0.Float = m_fAdditiveBasePhase;
		priorityLow.rate0.Float = m_fAdditiveBaseRate;
		priorityLow.weight0.Float = 1.0f;

		priorityLow.dict1.String = "";
		priorityLow.clip1.String = "";
		priorityLow.phase1.Float = 0.0f;
		priorityLow.rate1.Float = 0.0f;
		priorityLow.weight1.Float = 0.0f;

		priorityLow.dict2.String = "";
		priorityLow.clip2.String = "";
		priorityLow.phase2.Float = 0.0f;
		priorityLow.rate2.Float = 0.0f;
		priorityLow.weight2.Float = 0.0f;

		priorityLow.filter.Int = 0;
		//priorityLow.blendInDuration;
		//priorityLow.blendOutDuration;
		//priorityLow.timeToPlay;
		priorityLow.flags.Int |= m_bAdditiveBaseLooping ? BIT(AF_LOOPING) : 0;
		priorityLow.flags.Int |= BIT(AF_SECONDARY);
		//priorityLow.ikFlags;

		CTaskScriptedAnimation::ScriptInitSlotData priorityMid;

		priorityMid.state.Int = CTaskScriptedAnimation::kStateSingleClip;

		priorityMid.dict0.String = m_additiveOverlaySelector.GetSelectedClipDictionary();
		priorityMid.clip0.String = m_additiveOverlaySelector.GetSelectedClipName();
		priorityMid.phase0.Float = m_fAdditiveOverlayPhase;
		priorityMid.rate0.Float = m_fAdditiveOverlayRate;
		priorityMid.weight0.Float = 1.0f;

		priorityMid.dict1.String = "";
		priorityMid.clip1.String = "";
		priorityMid.phase1.Float = 0.0f;
		priorityMid.rate1.Float = 0.0f;
		priorityMid.weight1.Float = 0.0f;

		priorityMid.dict2.String = "";
		priorityMid.clip2.String = "";
		priorityMid.phase2.Float = 0.0f;
		priorityMid.rate2.Float = 0.0f;
		priorityMid.weight2.Float = 0.0f;

		priorityMid.filter.Int = 0;
		//priorityMid.blendInDuration;
		//priorityMid.blendOutDuration;
		//priorityMid.timeToPlay;
		priorityMid.flags.Int |= m_bAdditiveBaseLooping ? BIT(AF_LOOPING) : 0;
		priorityMid.flags.Int |= BIT(AF_SECONDARY);
		priorityMid.flags.Int |= BIT(AF_ADDITIVE);
		//priorityMid.ikFlags;

		CTaskScriptedAnimation::ScriptInitSlotData priorityHigh;

		//priorityHigh.state;

		priorityHigh.dict0.String = "";
		priorityHigh.clip0.String = "";
		priorityHigh.phase0.Float = 0.0f;
		priorityHigh.rate0.Float = 0.0f;
		priorityHigh.weight0.Float = 0.0f;

		priorityHigh.dict1.String = "";
		priorityHigh.clip1.String = "";
		priorityHigh.phase1.Float = 0.0f;
		priorityHigh.rate1.Float = 0.0f;
		priorityHigh.weight1.Float = 0.0f;

		priorityHigh.dict2.String = "";
		priorityHigh.clip2.String = "";
		priorityHigh.phase2.Float = 0.0f;
		priorityHigh.rate2.Float = 0.0f;
		priorityHigh.weight2.Float = 0.0f;

		priorityHigh.filter.Int = 0;
		//priorityHigh.blendInDuration;
		//priorityHigh.blendOutDuration;
		//priorityHigh.timeToPlay;
		priorityHigh.flags.Int |= m_bAdditiveBaseLooping ? BIT(AF_LOOPING) : 0;
		priorityHigh.flags.Int |= BIT(AF_SECONDARY);
		//priorityHigh.ikFlags;

		float fBlendInDuration = INSTANT_BLEND_DURATION;
		float fBlendOutDuration = INSTANT_BLEND_DURATION;

		CTask* pTask = rage_new CTaskScriptedAnimation(priorityLow, priorityMid, priorityHigh, fBlendInDuration, fBlendOutDuration);

		CObject *pObject = static_cast<CObject*>(m_pDynamicEntity.Get());
		pObject->SetTask(pTask, CObjectIntelligence::OBJECT_TASK_TREE_SECONDARY, CObjectIntelligence::OBJECT_TASK_SECONDARY_ANIM);
	}
}

void CAnimViewer::StopObjectAdditiveAnim()
{
	if (m_pDynamicEntity &&
		m_pDynamicEntity->GetType() == ENTITY_TYPE_OBJECT)
	{
		CObject *pObject = static_cast<CObject*>(m_pDynamicEntity.Get());
		pObject->GetObjectIntelligence()->GetTaskManager()->ClearTask(CObjectIntelligence::OBJECT_TASK_TREE_SECONDARY, CObjectIntelligence::OBJECT_TASK_SECONDARY_ANIM);
	}
}

#if CRMT_NODE_2_DEF_MAP

void WalkMotionTree(u32 networkHash, const crmtNode *pNode, atString &data, float weight, u32 networkHashFilter)
{
	// Get node hash
	u32 nodeHash = 0;
	const void **ppNodeDef = g_crmtNode2DefMap.Access(pNode);
	Assert(ppNodeDef);
	if(ppNodeDef)
	{
		const mvNodeDef *pNodeDef = reinterpret_cast< const mvNodeDef * >(*ppNodeDef);
		if(pNodeDef)
		{
			nodeHash = pNodeDef->m_id;
		}
	}

	if(networkHash == networkHashFilter != 0 && nodeHash != 0)
	{
		// Add this node to data
		data += atVarString("%u\t%u\t%f\n", networkHash, nodeHash, weight);
	}

	// Update network name
	if(pNode->GetNodeType() == crmtNode::kNodeSubNetwork)
	{
		const mvNodeSubNetwork* pNodeSubNetwork = static_cast< const mvNodeSubNetwork * >(pNode);

		const mvSubNetwork *pSubNetwork = pNodeSubNetwork->DebugGetSubNetwork();
		if(pSubNetwork)
		{
			const mvNetworkDef *pNetworkDef = pSubNetwork->GetDefinition();
			Assert(pNetworkDef);
			if(pNetworkDef)
			{
				const char *szNetworkDefName = fwAnimDirector::FindNetworkDefName(pNetworkDef);
				Assert(szNetworkDefName);
				if(szNetworkDefName)
				{
					networkHash = atStringHash(szNetworkDefName);
				}
			}
		}
		else
		{
			networkHash = pNodeSubNetwork->DebugGetNetworkId();
		}
	}

	// Iterate through child nodes
	for(int i = 0, count = pNode->GetNumChildren(); i < count; i ++)
	{
		const crmtNode *pChildNode = pNode->GetChild(i);
		if(pChildNode)
		{
			float childWeight = pNode->GetChildInfluence(i);

			WalkMotionTree(networkHash, pChildNode, data, weight * childWeight, networkHashFilter);
		}
	}
}

void CAnimViewer::DumpNetworkNodePropertyData()
{
	if(m_pNetworkNodePropertyData)
	{
		atString data(m_networkDebugData);
		atArray< atString > split; data.Split(split, "\n", true);
		if(split.GetCount() > 0)
		{
			for(int i = 0, count = split.GetCount(); i < count; i ++)
			{
				atArray< atString > split2; split[i].Split(split2, "\t", true);
				atHashString network; network.SetHash(strtoul(split2[0], NULL, 0));
				atHashString node; node.SetHash(strtoul(split2[1], NULL, 0));
				atString weight = split2[2];
				animDisplayf("%i: %s %u %s %u %s",
					i,
					network.TryGetCStr(), network.GetHash(),
					node.TryGetCStr(), node.GetHash(),
					weight.c_str());
			}
			animDisplayf("");

			return;
		}
	}

	animDisplayf("No data");
}

void CAnimViewer::UpdateNetworkNodePropertyData()
{
	if(PARAM_movedebug.Get())
	{
		if(m_bEnableNetworksDebug && m_pDynamicEntity && m_pNetworkNodePropertyData)
		{
			u32 networkHashFilter = 0;
			size_t networkFilterLength = strlen(m_networkDebugNetworkFilter);
			if(networkFilterLength > 0)
			{
				bool isHash = true;
				for(size_t i = 0; i < networkFilterLength && isHash; i ++)
				{
					if(!isdigit(m_networkDebugNetworkFilter[i]))
					{
						isHash = false;
					}
				}
				if(isHash)
				{
					networkHashFilter = strtoul(m_networkDebugNetworkFilter, NULL, 0);
				}
				else
				{
					networkHashFilter = atStringHash(m_networkDebugNetworkFilter);
				}
			}

			fwAnimDirector *pAnimDirector = m_pDynamicEntity->GetAnimDirector();
			if(pAnimDirector)
			{
				fwMove *pMove = &pAnimDirector->GetMove();
				if(pMove)
				{
					u32 networkHash = 0;
					mvNetwork *pNetwork = pMove->GetNetwork();
					if(pNetwork)
					{
						const mvNetworkDef *pNetworkDef = pNetwork->GetDefinition();
						if(pNetworkDef)
						{
							atString networkNameString(pNetworkDef->GetName());
							if(networkNameString.StartsWith("memory:$"))
							{
								int index = networkNameString.LastIndexOf(":");
								if(index != -1)
								{
									atString tempString(&networkNameString.c_str()[index + 1]);
									networkNameString = tempString;
								}
							}
							if(networkNameString.GetLength() > 0)
							{
								networkHash = atStringHash(networkNameString.c_str());
							}
						}
					}

					if(networkHash != 0)
					{
						crmtMotionTree *pRootMotionTree = &pMove->GetMotionTree();
						if(pRootMotionTree)
						{
							crmtNode *pRootNode = pRootMotionTree->GetRootNode();
							if(pRootNode)
							{
								atString data;
								WalkMotionTree(networkHash, pRootNode, data, 1.0f, networkHashFilter);
								if(data.c_str())
								{
									strcpy(m_networkDebugData, data.c_str());
								}
								else
								{
									strcpy(m_networkDebugData, "");
								}
							}
						}
					}
				}
			}
		}
	}
}

#endif //CRMT_NODE_2_DEF_MAP

// Motion tree capture

void CAnimViewer::LoadMotionTreeCapture()
{
	Assertf(strlen(m_szMotionTreeCapture) > 0, "Motion tree capture filename must not be empty!");
	if(strlen(m_szMotionTreeCapture) > 0)
	{
		m_bCaptureMotionTree = false;
		m_bCapturingMotionTree = false;
		m_MoveDump->Reset();

		Assertf(m_MoveDump->Load(m_szMotionTreeCapture), "Could not load motion tree capture \"%s\"!", m_szMotionTreeCapture);
	}
}

void CAnimViewer::SaveMotionTreeCapture()
{
	Assertf(strlen(m_szMotionTreeCapture) > 0, "Motion tree capture filename must not be empty!");
	if(strlen(m_szMotionTreeCapture) > 0)
	{
		Assertf(m_MoveDump->Save(m_szMotionTreeCapture), "Could not save motion tree capture \"%s\"!", m_szMotionTreeCapture);
	}
}

void CAnimViewer::PrintMotionTreeCapture()
{
	Assertf(strlen(m_szMotionTreeCapture) > 0, "Motion tree capture filename must not be empty!");
	if(strlen(m_szMotionTreeCapture) > 0)
	{
		u32 uFrameIndex = 0;

		if(m_MoveDump->GetFrameCount() > 0)
		{
			if(m_bCaptureMotionTree)
			{
				uFrameIndex = m_MoveDump->GetFrameCount() - 1;
			}
			else
			{
				uFrameIndex = Min(m_uRenderMotionTreeCaptureFrame, m_MoveDump->GetFrameCount() - 1);

			}
		}

		fwMoveDumpRenderFlags renderFlags(fwMotionTreeVisualiser::ms_bRenderTree,
			fwMotionTreeVisualiser::ms_bRenderClass,
			fwMotionTreeVisualiser::ms_bRenderWeight,
			fwMotionTreeVisualiser::ms_bRenderLocalWeight,
			fwMotionTreeVisualiser::ms_bRenderStateActiveName,
			fwMotionTreeVisualiser::ms_bRenderRequests,
			fwMotionTreeVisualiser::ms_bRenderFlags,
			fwMotionTreeVisualiser::ms_bRenderInputParams,
			fwMotionTreeVisualiser::ms_bRenderOutputParams,
			fwMotionTreeVisualiser::ms_bRenderInvalidNodes,
			fwMotionTreeVisualiser::ms_bRenderSynchronizerWeights,
			fwMotionTreeVisualiser::ms_bRenderSynchronizerGraph,
			fwMotionTreeVisualiser::ms_bRenderDofs,
			fwMotionTreeVisualiser::ms_bRenderAddress);

		Assertf(m_MoveDump->Print(m_szMotionTreeCapture, uFrameIndex, renderFlags), "Could not print motion tree capture \"%s\"!", m_szMotionTreeCapture);
	}
}

void CAnimViewer::PrintAllMotionTreeCapture()
{
	Assertf(strlen(m_szMotionTreeCapture) > 0, "Motion tree capture filename must not be empty!");
	if(strlen(m_szMotionTreeCapture) > 0)
	{
		fwMoveDumpRenderFlags renderFlags(fwMotionTreeVisualiser::ms_bRenderTree,
			fwMotionTreeVisualiser::ms_bRenderClass,
			fwMotionTreeVisualiser::ms_bRenderWeight,
			fwMotionTreeVisualiser::ms_bRenderLocalWeight,
			fwMotionTreeVisualiser::ms_bRenderStateActiveName,
			fwMotionTreeVisualiser::ms_bRenderRequests,
			fwMotionTreeVisualiser::ms_bRenderFlags,
			fwMotionTreeVisualiser::ms_bRenderInputParams,
			fwMotionTreeVisualiser::ms_bRenderOutputParams,
			fwMotionTreeVisualiser::ms_bRenderInvalidNodes,
			fwMotionTreeVisualiser::ms_bRenderSynchronizerWeights,
			fwMotionTreeVisualiser::ms_bRenderSynchronizerGraph,
			fwMotionTreeVisualiser::ms_bRenderDofs,
			fwMotionTreeVisualiser::ms_bRenderAddress);

		Assertf(m_MoveDump->Print(m_szMotionTreeCapture, renderFlags), "Could not print motion tree capture \"%s\"!", m_szMotionTreeCapture);
	}
}

#if CALLSTACK_ON_BLOCKING_ANIM_PREPHYSICS_UPDATE
void CAnimViewer::PotentialMotionTreeBlockCallstackCollector()
{
	fwAnimDirectorComponentMotionTree::sm_LockedGlobalCallStacks.PrintInfoForTag(0);
}
#endif //CALLSTACK_ON_BLOCKING_ANIM_PREPHYSICS_UPDATE

// Entity capture

void CAnimViewer::EntityCaptureAddEntity()
{
	if(m_pDynamicEntity.Get() && m_pBank)
	{
		/* Check that we haven't already added this entity to the capture */
		for(int i = 0; i < m_EntityCaptureArray.GetCount(); i ++)
		{
			if(m_EntityCaptureArray[i]->GetEntity() == m_pDynamicEntity)
			{
				/* We've already added this entity to the capture */
				return;
			}
		}

		/* Add this entity to the capture */
		m_EntityCaptureArray.push_back(rage_new CEntityCapture(m_pDynamicEntity, m_pBank, m_pEntitiesToCaptureGroup));
	}
}

void CAnimViewer::EntityCaptureUpdate()
{
	for(int i = 0; i < m_EntityCaptureArray.GetCount(); i ++)
	{
		CEntityCapture *pEntityCapture = m_EntityCaptureArray[i];
		if(pEntityCapture)
		{
			pEntityCapture->Update();
		}
	}
}

void CAnimViewer::EntityCaptureToggleLock30Fps()
{
	if(m_EntityCaptureLock30Fps)
	{
		if(m_EntityCaptureLock60Fps)
		{
			m_EntityCaptureLock60Fps = false;

			fwTimer::UnsetForcedFps();
		}

		fwTimer::SetForcedFps(30.0f);
	}
	else
	{
		fwTimer::UnsetForcedFps();
	}
}

void CAnimViewer::EntityCaptureToggleLock60Fps()
{
	if(m_EntityCaptureLock60Fps)
	{
		if(m_EntityCaptureLock30Fps)
		{
			m_EntityCaptureLock30Fps = false;

			fwTimer::UnsetForcedFps();
		}

		fwTimer::SetForcedFps(60.0f);
	}
	else
	{
		fwTimer::UnsetForcedFps();
	}
}

void CAnimViewer::EntityCaptureStart()
{
	for(int i = 0; i < m_EntityCaptureArray.GetCount(); i ++)
	{
		CEntityCapture *pEntityCapture = m_EntityCaptureArray[i];
		if(pEntityCapture)
		{
			pEntityCapture->Start();
		}
	}
}

void CAnimViewer::EntityCaptureRenderUpdate()
{
	for(int i = 0; i < m_EntityCaptureArray.GetCount(); i ++)
	{
		CEntityCapture *pEntityCapture = m_EntityCaptureArray[i];
		if(pEntityCapture)
		{
			pEntityCapture->RenderUpdate();
		}
	}
}
void CAnimViewer::EntityCaptureStop()
{
	for(int i = 0; i < m_EntityCaptureArray.GetCount(); i ++)
	{
		CEntityCapture *pEntityCapture = m_EntityCaptureArray[i];
		if(pEntityCapture)
		{
			pEntityCapture->Stop();
		}
	}
}


void CAnimViewer::EntityCaptureSingleFrame()
{
	for(int i = 0; i < m_EntityCaptureArray.GetCount(); i ++)
	{
		CEntityCapture *pEntityCapture = m_EntityCaptureArray[i];
		if(pEntityCapture)
		{
			pEntityCapture->SingleFrame();
		}
	}
}

// Camera capture

void CAnimViewer::CameraCapture_CameraNameIndexChanged()
{
	if(m_iCameraCapture_CameraNameIndex >= 0 && m_iCameraCapture_CameraNameIndex < m_iCameraCapture_CameraNameCount)
	{
		const char *szCameraName = m_szCameraCapture_CameraNames[m_iCameraCapture_CameraNameIndex];
		if(animVerifyf(szCameraName && strlen(szCameraName) > 0, "Camera does not have a name!"))
		{
			camBaseCamera::Pool *pBaseCameraPool = camBaseCamera::GetPool();
			if(animVerifyf(pBaseCameraPool, "Could not get camera pool!"))
			{
				for(int i = 0; i < pBaseCameraPool->GetSize(); i ++)
				{
					camBaseCamera *pBaseCamera = pBaseCameraPool->GetSlot(i);
					if(pBaseCamera && pBaseCamera->GetName() && stricmp(pBaseCamera->GetName(), szCameraName) == 0)
					{
						m_iCameraCapture_CameraIndex = i;

						return;
					}
				}

				animAssertf(false, "Could not find a camera with the specified name!");
			}
		}
	}
	else
	{
		m_iCameraCapture_CameraNameIndex = 0;
	}
}

void CAnimViewer::CameraCapture_Start(int cameraIndex, const char *animName)
{
	camBaseCamera::Pool *pBaseCameraPool = camBaseCamera::GetPool();
	if(animVerifyf(pBaseCameraPool, "Could not get camera pool!"))
	{
		camBaseCamera *pBaseCamera = pBaseCameraPool->GetSlot(cameraIndex);
		if(animVerifyf(pBaseCamera, "Could not get specified camera!"))
		{
			if(animVerifyf(animName && strlen(animName) > 0, "No animation name was specified!"))
			{
				m_iCameraCapture_CameraIndex = cameraIndex;

				strcpy(m_szCameraCapture_AnimationName, animName);

				CameraCapture_Start();
			}
		}
	}
}

void CAnimViewer::CameraCapture_Start()
{
	if(animVerifyf(!m_bCameraCapture_Started, "Camera capture already started!"))
	{
		if(animVerifyf(strlen(m_szCameraCapture_AnimationName) > 0, "Camera capture name must not be empty!"))
		{
			if(animVerifyf(strchr(m_szCameraCapture_AnimationName, '.') == NULL, "Camera capture name should not have an extension, these will be added automatically!"))
			{
				camBaseCamera::Pool *pBaseCameraPool = camBaseCamera::GetPool();
				if(animVerifyf(pBaseCameraPool, "Could not get camera pool!"))
				{
					camBaseCamera *pBaseCamera = pBaseCameraPool->GetSlot(m_iCameraCapture_CameraIndex);
					if(animVerifyf(pBaseCamera, "Could not get specified camera!"))
					{
						animDisplayf("Capturing camera...\n");

						// Start camera capture
						m_bCameraCapture_Started = true;
					}
				}
			}
		}
	}
}

void CAnimViewer::CameraCapture_Update()
{
	if(animVerifyf(m_bCameraCapture_Started, "Camera capture has not been started!"))
	{
		if(animVerifyf(strlen(m_szCameraCapture_AnimationName) > 0, "Camera capture name must not be empty!"))
		{
			if(animVerifyf(strchr(m_szCameraCapture_AnimationName, '.') == NULL, "Camera capture name should not have an extension, these will be added automatically!"))
			{
				camBaseCamera::Pool *pBaseCameraPool = camBaseCamera::GetPool();
				if(animVerifyf(pBaseCameraPool, "Could not get camera pool!"))
				{
					camBaseCamera *pBaseCamera = pBaseCameraPool->GetSlot(m_iCameraCapture_CameraIndex);
					if(animVerifyf(pBaseCamera, "Could not get specified camera!"))
					{
						crFrame *pFrame = rage_new crFrame;
						if(animVerifyf(pFrame, "Could not allocate new frame!"))
						{
							crFrameData *pFrameData = rage_new crFrameData;
							if(animVerifyf(pFrameData, "Could not allocate new frame data!"))
							{
								const int numDofs = 6;

								u8 tracks[] = {
									kTrackCameraTranslation, //7
									kTrackCameraRotation, //8
									kTrackCameraFieldOfView, //27
									kTrackCameraDepthOfField, //28
									kTrackCameraDepthOfFieldStrength, //36
									kTrackCameraShallowDepthOfField, //39
								};
								CompileTimeAssert(COUNTOF(tracks) == numDofs);

								u16 ids[] = {
									0, //kTrackCameraTranslation
									0, //kTrackCameraRotation
									0, //kTrackCameraFieldOfView
									0, //kTrackCameraDepthOfField
									0, //kTrackCameraDepthOfFieldStrength
									0, //kTrackCameraShallowDepthOfField
								};
								CompileTimeAssert(COUNTOF(ids) == numDofs);

								u8 types[] = {
									kFormatTypeVector3, //kTrackCameraTranslation
									kFormatTypeQuaternion, //kTrackCameraRotation
									kFormatTypeFloat, //kTrackCameraFieldOfView
									kFormatTypeVector3, //kTrackCameraDepthOfField
									kFormatTypeFloat, //kTrackCameraDepthOfFieldStrength
									kFormatTypeFloat, //kTrackCameraShallowDepthOfField
								};
								CompileTimeAssert(COUNTOF(types) == numDofs);

								const camFrame &baseCameraFrame = pBaseCamera->GetFrame();

								crFrameDataInitializerDofs frameDataInitializerDofs(numDofs, tracks, ids, types);
								frameDataInitializerDofs.InitializeFrameData(*pFrameData);
								pFrame->Init(*pFrameData);

								Vector3 vTranslation = baseCameraFrame.GetPosition();
								pFrame->SetValue< Vec3V >(kTrackCameraTranslation, 0, VECTOR3_TO_VEC3V(vTranslation));

								Quaternion qRotation; qRotation.FromMatrix34(baseCameraFrame.GetWorldMatrix());
								pFrame->SetValue< QuatV >(kTrackCameraRotation, 0, QUATERNION_TO_QUATV(qRotation));

								float fFieldOfView = baseCameraFrame.GetFov();
								pFrame->SetValue< float >(kTrackCameraFieldOfView, 0, fFieldOfView);

								Vector3 vDepthOfField(baseCameraFrame.GetNearInFocusDofPlane(), baseCameraFrame.GetFarInFocusDofPlane(), 0.0f);
								pFrame->SetValue< Vec3V >(kTrackCameraDepthOfField, 0, VECTOR3_TO_VEC3V(vDepthOfField));

								float fDepthOfFieldStrength = baseCameraFrame.GetDofStrength();
								pFrame->SetValue< float >(kTrackCameraDepthOfFieldStrength, 0, fDepthOfFieldStrength);

								float fShallowDepthOfField = baseCameraFrame.GetFlags().IsFlagSet(camFrame::Flag_ShouldUseLightDof) ? 1.0f : 0.0f;
								pFrame->SetValue< float >(kTrackCameraShallowDepthOfField, 0, fShallowDepthOfField);

								// Add frame to array
								m_CameraCapture_FrameArray.PushAndGrow(pFrame, 1);

								animDisplayf("Captured camera frame %i...\n", m_CameraCapture_FrameArray.GetCount());
							}
						}
					}
				}
			}
		}
	}
}

void CAnimViewer::CameraCapture_Stop()
{
	if(animVerifyf(m_bCameraCapture_Started, "Camera capture has not been started!"))
	{
		if(animVerifyf(strlen(m_szCameraCapture_AnimationName) > 0, "Camera capture name must not be empty!"))
		{
			if(animVerifyf(strchr(m_szCameraCapture_AnimationName, '.') == NULL, "Camera capture name should not have an extension, these will be added automatically!"))
			{
				// Check we have at least two frames to save
				if(animVerifyf(m_CameraCapture_FrameArray.GetCount() > 1, "Camera capture needs at least two frames to save!"))
				{
					// Stop camera capture
					m_bCameraCapture_Started = false;

					// Calculate duration
					float duration = (m_CameraCapture_FrameArray.GetCount() - 1) * (1.0f / 30.0f);

					// Create animation
					crAnimation animation;
					if(animVerifyf(animation.Create(m_CameraCapture_FrameArray.GetCount(), duration), "Camera capture could not create animation!"))
					{
						animation.CreateFromFrames(m_CameraCapture_FrameArray, LOSSLESS_ANIM_TOLERANCE);
						animation.SetName(m_szCameraCapture_AnimationName);

						ASSET.PushFolder("X:/gta5");

						// Save animation
						animDisplayf("Saving animation '%s'\n...", atVarString("%s.anim", m_szCameraCapture_AnimationName).c_str());
						if(animVerifyf(animation.Save(atVarString("%s.anim", m_szCameraCapture_AnimationName).c_str()), "Camera capture could not save animation!"))
						{
							animDisplayf("done.\n");

							// Create clip
							crClipAnimation clip;
							if(animVerifyf(clip.Create(animation), "Camera capture could not create clip!"))
							{
								// Save clip
								animDisplayf("Saving clip '%s'\n...", atVarString("%s.clip", m_szCameraCapture_AnimationName).c_str());
								animVerifyf(clip.Save(atVarString("%s.clip", m_szCameraCapture_AnimationName).c_str()), "Camera capture could not save clip!");
								animDisplayf("done.\n");
							}
						}

						ASSET.PopFolder();
					}

					// Release frames
					for(int i = 0; i < m_CameraCapture_FrameArray.GetCount(); i ++)
					{
						// Release frame
						delete m_CameraCapture_FrameArray[i]; m_CameraCapture_FrameArray[i] = NULL;
					}
					m_CameraCapture_FrameArray.Reset();
				}
			}
		}
	}
}

void CAnimViewer::CameraCapture_SingleFrame()
{
	if(animVerifyf(strlen(m_szCameraCapture_AnimationName) > 0, "Camera capture name must not be empty!"))
	{
		if(animVerifyf(strchr(m_szCameraCapture_AnimationName, '.') == NULL, "Camera capture name should not have an extension, these will be added automatically!"))
		{
			camBaseCamera::Pool *pBaseCameraPool = camBaseCamera::GetPool();
			if(animVerifyf(pBaseCameraPool, "Could not get camera pool!"))
			{
				camBaseCamera *pBaseCamera = pBaseCameraPool->GetSlot(m_iCameraCapture_CameraIndex);
				if(animVerifyf(pBaseCamera, "Could not get specified camera!"))
				{
					crFrame *pFrame = rage_new crFrame;
					if(animVerifyf(pFrame, "Could not allocate new frame!"))
					{
						crFrameData *pFrameData = rage_new crFrameData;
						if(animVerifyf(pFrameData, "Could not allocate new frame data!"))
						{
							const int numDofs = 6;

							u8 tracks[] = {
								kTrackCameraTranslation, //7
								kTrackCameraRotation, //8
								kTrackCameraFieldOfView, //27
								kTrackCameraDepthOfField, //28
								kTrackCameraDepthOfFieldStrength, //36
								kTrackCameraShallowDepthOfField, //39
							};
							CompileTimeAssert(COUNTOF(tracks) == numDofs);

							u16 ids[] = {
								0, //kTrackCameraTranslation
								0, //kTrackCameraRotation
								0, //kTrackCameraFieldOfView
								0, //kTrackCameraDepthOfField
								0, //kTrackCameraDepthOfFieldStrength
								0, //kTrackCameraShallowDepthOfField
							};
							CompileTimeAssert(COUNTOF(ids) == numDofs);

							u8 types[] = {
								kFormatTypeVector3, //kTrackCameraTranslation
								kFormatTypeQuaternion, //kTrackCameraRotation
								kFormatTypeFloat, //kTrackCameraFieldOfView
								kFormatTypeVector3, //kTrackCameraDepthOfField
								kFormatTypeFloat, //kTrackCameraDepthOfFieldStrength
								kFormatTypeFloat, //kTrackCameraShallowDepthOfField
							};
							CompileTimeAssert(COUNTOF(types) == numDofs);

							const camFrame &baseCameraFrame = pBaseCamera->GetFrame();

							crFrameDataInitializerDofs frameDataInitializerDofs(numDofs, tracks, ids, types);
							frameDataInitializerDofs.InitializeFrameData(*pFrameData);
							pFrame->Init(*pFrameData);

							Vector3 vTranslation = baseCameraFrame.GetPosition();
							pFrame->SetValue< Vec3V >(kTrackCameraTranslation, 0, VECTOR3_TO_VEC3V(vTranslation));

							Quaternion qRotation; qRotation.FromMatrix34(baseCameraFrame.GetWorldMatrix());
							pFrame->SetValue< QuatV >(kTrackCameraRotation, 0, QUATERNION_TO_QUATV(qRotation));

							float fFieldOfView = baseCameraFrame.GetFov();
							pFrame->SetValue< float >(kTrackCameraFieldOfView, 0, fFieldOfView);

							Vector3 vDepthOfField(baseCameraFrame.GetNearInFocusDofPlane(), baseCameraFrame.GetFarInFocusDofPlane(), 0.0f);
							pFrame->SetValue< Vec3V >(kTrackCameraDepthOfField, 0, VECTOR3_TO_VEC3V(vDepthOfField));

							float fDepthOfFieldStrength = baseCameraFrame.GetDofStrength();
							pFrame->SetValue< float >(kTrackCameraDepthOfFieldStrength, 0, fDepthOfFieldStrength);

							float fShallowDepthOfField = baseCameraFrame.GetFlags().IsFlagSet(camFrame::Flag_ShouldUseLightDof) ? 1.0f : 0.0f;
							pFrame->SetValue< float >(kTrackCameraShallowDepthOfField, 0, fShallowDepthOfField);

							// Add frame to array
							m_CameraCapture_FrameArray.PushAndGrow(pFrame, 1);

							// Add frame to array
							m_CameraCapture_FrameArray.PushAndGrow(pFrame, 1);

							animDisplayf("Capturing single camera frame...\n");

							// Calculate duration
							float duration = (m_CameraCapture_FrameArray.GetCount() - 1) * (1.0f / 30.0f);

							// Create animation
							crAnimation animation;
							if(animVerifyf(animation.Create(m_CameraCapture_FrameArray.GetCount(), duration), "Camera capture could not create animation!"))
							{
								animation.CreateFromFrames(m_CameraCapture_FrameArray, LOSSLESS_ANIM_TOLERANCE);
								animation.SetName(m_szCameraCapture_AnimationName);

								ASSET.PushFolder("X:/gta5");

								// Save animation
								animDisplayf("Saving animation '%s'\n...", atVarString("%s.anim", m_szCameraCapture_AnimationName).c_str());
								if(animVerifyf(animation.Save(atVarString("%s.anim", m_szCameraCapture_AnimationName).c_str()), "Camera capture could not save animation!"))
								{
									animDisplayf("done.\n");

									// Create clip
									crClipAnimation clip;
									if(animVerifyf(clip.Create(animation), "Camera capture could not create clip!"))
									{
										// Save clip
										animDisplayf("Saving clip '%s'\n...", atVarString("%s.clip", m_szCameraCapture_AnimationName).c_str());
										animVerifyf(clip.Save(atVarString("%s.clip", m_szCameraCapture_AnimationName).c_str()), "Camera capture could not save clip!");
										animDisplayf("done.\n");
									}
								}

								ASSET.PopFolder();
							}

							// Release frames (the same frame has been added twice to the array!)
							delete m_CameraCapture_FrameArray[0]; m_CameraCapture_FrameArray[0] = NULL;

							// Release frame array
							m_CameraCapture_FrameArray.Reset();
						}
					}
				}
			}
		}
	}
}

CAnimViewer::BlendOutData::BlendOutData() :
m_MatchedSet(NMBS_INVALID),
m_MatchedItem(NULL),
m_RagdollFrame(NULL),
m_SelectedFrame(NULL),
m_MatchedFrame(NULL),
m_SkeletonData(NULL),
m_RagdollTransform(V_IDENTITY),
m_SelectedTransform(V_IDENTITY),
m_MatchedTransform(V_IDENTITY),
m_LowLODFrameFilter(NULL),
m_BlendOutGroup(NULL),
m_SelectedBlendOutSet(0),
m_SelectedBlendOut(0),
m_BlendOutSet(0),
m_DrawSelectedBlendOutSkeleton(false),
m_DrawMatchedBlendOutSkeleton(false),
m_DrawRagdollFrameSkeleton(false),
m_UseLowLODFilter(true)
{
}

void CAnimViewer::BlendOutData::Init(const CDynamicEntity& entity, const CNmBlendOutSetList& blendOutSetList, 
									 eNmBlendOutSet matchedSet, CNmBlendOutItem* matchedItem, const char* preTitle)
{
	m_BlendOutSetList = blendOutSetList;
	m_MatchedSet = matchedSet;
	m_MatchedItem = matchedItem;
	if (m_MatchedItem != NULL)
	{
		m_MatchedClipSet = m_MatchedItem->GetClipSet();
		m_MatchedClip = m_MatchedItem->GetClip();
	}
	char str[128];
	formatf(str, sizeof(str), "[%d] Blend Out For %s%s", fwTimer::GetGameTimer().GetFrameCount(), preTitle != NULL ? preTitle : "", entity.GetModelName());
	m_Title = str;
	m_LowLODFrameFilter = entity.GetLowLODFilter();

	fwAnimDirector* animDirector = entity.GetAnimDirector();
	if (animVerifyf(animDirector != NULL && animDirector->GetFrameBuffer().GetFrameData() != NULL, "CAnimViewer::BlendOutData::Init: Invalid anim director"))
	{
		const crFrameData& frameData = *animDirector->GetFrameBuffer().GetFrameData();
		m_RagdollFrame = rage_new crFrame(frameData);
		m_SelectedFrame = rage_new crFrame(frameData);
		m_MatchedFrame = rage_new crFrame(frameData);
	}

	const crSkeleton* skeleton = entity.GetSkeleton();
	if (animVerifyf(skeleton != NULL, "CAnimViewer::BlendOutData::Init: Invalid skeleton"))
	{
		m_RagdollTransform = *skeleton->GetParentMtx();
		if (animVerifyf(m_RagdollFrame != NULL, "CAnimViewer::BlendOutData::Init: Invalid frame"))
		{
			m_RagdollFrame->InversePose(*skeleton);

			// Since we always pose an animated ped and the mover capsule doesn't really support being rotated about its X or Y axes
			// then let's incorporate any rotation about those axes into the frame itself
			QuatV rootQuaternion;
			if (m_RagdollFrame->GetQuaternion(kTrackBoneRotation, BONETAG_ROOT, rootQuaternion))
			{
				Vec3V ragdollTransformEulers = Mat34VToEulersXYZ(m_RagdollTransform);
				QuatV ragdollQuaternion;
				// Only want roll/pitch - heading and position can be applied to the mover capsule
				ragdollQuaternion = QuatVFromEulersXYZ(Vec3V(ragdollTransformEulers.GetXY(), ScalarV(V_ZERO)));
				rootQuaternion = Multiply(ragdollQuaternion, rootQuaternion);
				m_RagdollFrame->SetQuaternion(kTrackBoneRotation, BONETAG_ROOT, rootQuaternion);

				// Only want heading - roll/pitch have been applied to the frame directly
				Mat33VFromEulersXYZ(m_RagdollTransform.GetMat33Ref(), Vec3V(Vec2V(V_ZERO), ragdollTransformEulers.GetZ()));
			}
		}

		// See if we've already allocated a matching crSkeletonData
		u32 signatureNonChiral = skeleton->GetSkeletonData().GetSignatureNonChiral();
		for (atSNode<BlendOutData>* pNode = m_BlendOutData.GetHead(); pNode != NULL; pNode = pNode->GetNext())
		{
			if (&pNode->Data != this && signatureNonChiral == pNode->Data.m_SkeletonData->GetSignatureNonChiral())
			{
				m_SkeletonData = pNode->Data.m_SkeletonData;
				m_SkeletonData->AddRef();
				break;
			}
		}

		if (m_SkeletonData == NULL)
		{
			m_SkeletonData = rage_new crSkeletonData(skeleton->GetSkeletonData());
		}
	}

	const fwArchetype* archetype = entity.GetArchetype();
	if (animVerifyf(archetype != NULL, "CAnimViewer::BlendOutData::Init: Invalid archetype for entity %s", entity.GetModelName()))
	{
		strLocalIndex poseMatcherFileIndex = strLocalIndex(archetype->GetPoseMatcherFileIndex());
		if (poseMatcherFileIndex != -1)
		{
			crPoseMatcher* poseMatcher = g_PoseMatcherStore.Get(poseMatcherFileIndex);
			if (poseMatcher)
			{
				m_PoseMatcher.AddPoseMatcher(*poseMatcher);
			}
		}

		poseMatcherFileIndex = archetype->GetPoseMatcherProneFileIndex();
		if (poseMatcherFileIndex != -1)
		{
			crPoseMatcher* poseMatcher = g_PoseMatcherStore.Get(poseMatcherFileIndex);
			if (poseMatcher)
			{
				m_PoseMatcher.AddPoseMatcher(*poseMatcher);
			}
		}
	}

	OnMatchChanged();
}

CAnimViewer::BlendOutData::~BlendOutData()
{
	if (m_BlendOutGroup != NULL)
	{
		m_BlendOutGroup->Destroy();
		m_BlendOutGroup = NULL;
	}

	if (m_RagdollFrame != NULL)
	{
		animAssertf(m_RagdollFrame->GetRef() == 1, "CAnimViewer::BlendOutData::~BlendOutData: Invalid references for ragdoll frame");
		delete m_RagdollFrame;
		m_RagdollFrame = NULL;
	}

	if (m_SelectedFrame != NULL)
	{
		animAssertf(m_SelectedFrame->GetRef() == 1, "CAnimViewer::BlendOutData::~BlendOutData: Invalid references for selected frame");
		delete m_SelectedFrame;
		m_SelectedFrame = NULL;
	}

	if (m_MatchedFrame != NULL)
	{
		animAssertf(m_MatchedFrame->GetRef() == 1, "CAnimViewer::BlendOutData::~BlendOutData: Invalid references for matched frame");
		delete m_MatchedFrame;
		m_MatchedFrame = NULL;
	}

	if (m_SkeletonData != NULL)
	{
		m_SkeletonData->Release();
		m_SkeletonData = NULL;
	}
}

void CAnimViewer::OnPoseSelectedPedUsingCurrentlyDrawnSkelecton(BlendOutData* blendOutData)
{
	if (m_pDynamicEntity != NULL && m_pDynamicEntity->GetIsTypePed())
	{
		CPed* pPed = static_cast<CPed*>(m_pDynamicEntity.Get());

		fwAnimDirectorComponentRagDoll* pComponentRagDoll = pPed->GetAnimDirector()->GetComponent<fwAnimDirectorComponentRagDoll>();

		if (pComponentRagDoll != NULL)
		{
			crFrame* pCurrentlyDrawnSkeletonFrame = NULL;

			if (blendOutData->m_DrawRagdollFrameSkeleton && blendOutData->m_RagdollFrame != NULL)
			{
				pCurrentlyDrawnSkeletonFrame = blendOutData->m_RagdollFrame;
			}
			else if (blendOutData->m_DrawMatchedBlendOutSkeleton && blendOutData->m_MatchedFrame != NULL)
			{
				pCurrentlyDrawnSkeletonFrame = blendOutData->m_MatchedFrame;
			}
			else if (blendOutData->m_DrawSelectedBlendOutSkeleton && blendOutData->m_SelectedFrame != NULL)
			{
				pCurrentlyDrawnSkeletonFrame = blendOutData->m_SelectedFrame;
			}

			if (pCurrentlyDrawnSkeletonFrame != NULL)
			{
				fwAnimDirectorComponentRagDoll* pComponentRagDoll = pPed->GetAnimDirector()->GetComponent<fwAnimDirectorComponentRagDoll>();
				if (pComponentRagDoll != NULL)
				{
					pComponentRagDoll->SetFromFrame(*pCurrentlyDrawnSkeletonFrame);
					pPed->GetMovePed().SwitchToStaticFrame(0.0f);
				}
			}
		}
	}
}

void CAnimViewer::OnSetSelectedPedPositionUsingCurrentlyDrawnSkeleton(BlendOutData* blendOutData)
{
	if (m_pDynamicEntity != NULL && m_pDynamicEntity->GetIsTypePed())
	{
		CPed* pPed = static_cast<CPed*>(m_pDynamicEntity.Get());

		Matrix34 xCurrentlyDrawnSkeletonTransform;
		crFrame* pCurrentlyDrawnSkeletonFrame = NULL;
		pPed->GetMatrixCopy(xCurrentlyDrawnSkeletonTransform);

		if (blendOutData->m_DrawRagdollFrameSkeleton && blendOutData->m_RagdollFrame != NULL)
		{
			pCurrentlyDrawnSkeletonFrame = blendOutData->m_RagdollFrame;
			xCurrentlyDrawnSkeletonTransform = RCC_MATRIX34(blendOutData->m_RagdollTransform);
		}
		else if (blendOutData->m_DrawMatchedBlendOutSkeleton && blendOutData->m_MatchedFrame != NULL)
		{
			pCurrentlyDrawnSkeletonFrame = blendOutData->m_MatchedFrame;
			xCurrentlyDrawnSkeletonTransform = RCC_MATRIX34(blendOutData->m_MatchedTransform);
		}
		else if (blendOutData->m_DrawSelectedBlendOutSkeleton && blendOutData->m_SelectedFrame != NULL)
		{
			pCurrentlyDrawnSkeletonFrame = blendOutData->m_SelectedFrame;
			xCurrentlyDrawnSkeletonTransform = RCC_MATRIX34(blendOutData->m_SelectedTransform);
		}

		if (pCurrentlyDrawnSkeletonFrame != NULL)
		{
			xCurrentlyDrawnSkeletonTransform.RotateLocalX(CAnimViewer::m_BlendOutOrientationOffset.x * DtoR);
			xCurrentlyDrawnSkeletonTransform.RotateLocalY(CAnimViewer::m_BlendOutOrientationOffset.y * DtoR);
			xCurrentlyDrawnSkeletonTransform.RotateLocalZ(CAnimViewer::m_BlendOutOrientationOffset.z * DtoR);
			xCurrentlyDrawnSkeletonTransform.Translate(CAnimViewer::m_BlendOutTranslationOffset);

			pPed->SetMatrix(xCurrentlyDrawnSkeletonTransform);
			pPed->SetFixedPhysics(true);
			pPed->DisableCollision();
		}
	}
}

void CAnimViewer::ReactivateSelectedPed()
{
	if (m_pDynamicEntity != NULL && m_pDynamicEntity->GetIsTypePed())
	{
		CPed* pPed = static_cast<CPed*>(m_pDynamicEntity.Get());
		pPed->EnableCollision();
		pPed->SetFixedPhysics(false);
		pPed->GetPedIntelligence()->AddTaskAtPriority(rage_new CTaskDoNothing(10), PED_TASK_PRIORITY_PRIMARY , true);
		pPed->GetMovePed().SetTaskNetwork(NULL, 0.0f);
		pPed->GetMovePed().SwitchToAnimated(0.0f, false);
		// Force the motion state to restart the ped motion move network
		pPed->ForceMotionStateThisFrame(CPedMotionStates::MotionState_Idle, true);
	}
}

void CAnimViewer::OnForceSelectedPedIntoGetUpTask()
{
	if (m_pDynamicEntity != NULL && m_pDynamicEntity->GetIsTypePed())
	{
		CPed* pPed = static_cast<CPed*>(m_pDynamicEntity.Get());
		CEventGivePedTask event(PED_TASK_PRIORITY_PRIMARY, rage_new CTaskGetUp(true), false, E_PRIORITY_SCRIPT_COMMAND_SP);
		pPed->GetPedIntelligence()->AddEvent(event);
	}
}

void CAnimViewer::OnLoadRagdollPose(BlendOutData* blendOutData)
{
	ASSET.PushFolder("X:/gta5");

	if (animVerifyf(blendOutData->m_RagdollFrame != NULL, "CAnimViewer::OnLoadRagdollPose: Invalid ragdoll frame"))
	{
		crClip* clip = crClip::AllocateAndLoad(m_BlendOutAnimationName);

		if (animVerifyf(clip != NULL, "CAnimViewer::OnLoadRagdollPose: Could not load animation!"))
		{
			ASSERT_ONLY(bool composite = )clip->Composite(*blendOutData->m_RagdollFrame, 0.0f);
			animAssertf(composite, "CAnimViewer::OnLoadRagdollPose: Could not composite frame");

			crProperties* properties = clip->GetProperties();
			if (animVerifyf(properties != NULL, "CAnimViewer::OnLoadRagdollPose: Invalid clip properties"))
			{
				crProperty* ragdollPoseTransformProperty = properties->FindProperty("RagdollPose_DO_NOT_RESOURCE");
				if (animVerifyf(ragdollPoseTransformProperty != NULL, "CAnimViewer::OnLoadRagdollPose: Invalid ragdoll transform property"))
				{
					crPropertyAttribute* ragdollPoseTransformPropertyAttribute = ragdollPoseTransformProperty->GetAttribute("Transform");
					if (animVerifyf(ragdollPoseTransformPropertyAttribute != NULL && ragdollPoseTransformPropertyAttribute->GetType() == crPropertyAttribute::kTypeMatrix34, 
						"CAnimViewer::OnLoadRagdollPose: Invalid ragdoll transform property attribute"))
					{
						blendOutData->m_RagdollTransform = static_cast<crPropertyAttributeMatrix34*>(ragdollPoseTransformPropertyAttribute)->GetMatrix34();
					}
				}

				blendOutData->m_BlendOutSetList.Reset();
				
				crProperty* blendOutSetListProperty = properties->FindProperty("BlendOutSetList_DO_NOT_RESOURCE");
				if (animVerifyf(blendOutSetListProperty != NULL, "CAnimViewer::OnLoadRagdollPose: Invalid blend out set list property"))
				{
					for (int i = 0; i < blendOutSetListProperty->GetNumAttributes(); i++)
					{
						crPropertyAttribute* blendOutSetListPropertyAttribute = blendOutSetListProperty->GetAttributeByIndex(i);
						if (animVerifyf(blendOutSetListPropertyAttribute != NULL && blendOutSetListPropertyAttribute->GetType() == crPropertyAttribute::kTypeHashString, 
							"CAnimViewer::OnLoadRagdollPose: Invalid blend out set list property attribute"))
						{
							blendOutData->m_BlendOutSetList.Add(static_cast<crPropertyAttributeHashString*>(blendOutSetListPropertyAttribute)->GetHashString());
						}
					}
				}
			}

			delete clip;
			clip = NULL;
		}
	}

	ASSET.PopFolder();

	OnRecalculateMatch(blendOutData);

	OnSelectedBlendOutSetListChanged(blendOutData);
}

void CAnimViewer::OnSaveRagdollPose(BlendOutData* blendOutData)
{
	ASSET.PushFolder("X:/gta5");
	
	if (animVerifyf(blendOutData->m_RagdollFrame != NULL, "CAnimViewer::OnSaveRagdollPose: Invalid ragdoll frame"))
	{
		atArray<const crFrame*> frameArray;
		frameArray.PushAndGrow(blendOutData->m_RagdollFrame);
		frameArray.PushAndGrow(blendOutData->m_RagdollFrame);
		
		float time = (frameArray.GetCount() - 1) * (1.0f / 30.0f);

		crAnimation animation;
		if (animVerifyf(animation.Create(frameArray.GetCount(), time), "CAnimViewer::OnSaveRagdollPose: Could not create animation!"))
		{
			animation.CreateFromFramesFast(frameArray, LOSSLESS_ANIM_TOLERANCE);
			animation.SetName(m_BlendOutAnimationName);

			animDisplayf("Saving animation '%s.anim'...\n", m_BlendOutAnimationName);
			if (animVerifyf(animation.Save(atVarString("%s.anim", m_BlendOutAnimationName).c_str()), "CAnimViewer::OnSaveRagdollPose: Could not save animation!"))
			{
				animDisplayf("done.\n");

				crClipAnimation clip;
				if (animVerifyf(clip.Create(animation), "CAnimViewer::OnSaveRagdollPose: Could not create clip!"))
				{
					crProperties* properties = clip.GetProperties();
					if (animVerifyf(properties != NULL, "CAnimViewer::OnSaveRagdollPose: Invalid clip properties"))
					{
						crProperty ragdollPoseTransformProperty;
						ragdollPoseTransformProperty.SetName("RagdollPose_DO_NOT_RESOURCE");

						crPropertyAttributeMatrix34 ragdollPoseTransformPropertyAttribute;
						ragdollPoseTransformPropertyAttribute.SetName("Transform");
						ragdollPoseTransformPropertyAttribute.GetMatrix34() = blendOutData->m_RagdollTransform;
						ragdollPoseTransformProperty.AddAttribute(ragdollPoseTransformPropertyAttribute);

						properties->AddProperty(ragdollPoseTransformProperty);

						crProperty blendOutSetListProperty;
						blendOutSetListProperty.SetName("BlendOutSetList_DO_NOT_RESOURCE");

						char blendOutSetListName[32];
						for (int i = 0; i < blendOutData->m_BlendOutSetList.GetNumBlendOutSets(); i++)
						{
							formatf(blendOutSetListName, sizeof(blendOutSetListName), "%s%d", "BlendOutSet", i);
							crPropertyAttributeHashString blendOutSetListPropertyAttribute;
							blendOutSetListPropertyAttribute.SetName(blendOutSetListName);
							blendOutSetListPropertyAttribute.GetHashString() = blendOutData->m_BlendOutSetList.GetBlendOutSet(i);

							blendOutSetListProperty.AddAttribute(blendOutSetListPropertyAttribute);
						}

						properties->AddProperty(blendOutSetListProperty);
					}

					animDisplayf("Saving clip '%s.clip'...\n", m_BlendOutAnimationName);
					animVerifyf(clip.Save(atVarString("%s.clip", m_BlendOutAnimationName).c_str()), "CAnimViewer::OnSaveRagdollPose: Could not save clip!");
					animDisplayf("done.\n");
				}
			}
		}
	}
	
	ASSET.PopFolder();
}

void CAnimViewer::OnAddBlendOutSet(BlendOutData* blendOutData)
{
	blendOutData->m_BlendOutSetList.Add((eNmBlendOutSet)*g_NmBlendOutPoseManager.m_sets.GetKey(blendOutData->m_BlendOutSet));

	OnSelectedBlendOutSetListChanged(blendOutData);
}

void CAnimViewer::OnRemoveBlendOutSet(BlendOutData* blendOutData)
{
	// Unfortunately there's no interface for removing a blend out set from a list so we store existing entries, 
	// clear the blend out set list and then restore all but the removed entry to the list
	CNmBlendOutSetList blendOutSetList;
	eNmBlendOutSet blendOutSetToRemove = (eNmBlendOutSet)*g_NmBlendOutPoseManager.m_sets.GetKey(blendOutData->m_BlendOutSet);
	for (int i = 0; i < blendOutData->m_BlendOutSetList.GetNumBlendOutSets(); i++)
	{
		eNmBlendOutSet blendOutSet = blendOutData->m_BlendOutSetList.GetBlendOutSet(i);
		if (blendOutSet != blendOutSetToRemove)
		{
			blendOutSetList.Add(blendOutData->m_BlendOutSetList.GetBlendOutSet(i));
		}
	}

	blendOutData->m_BlendOutSetList.Reset();

	for (int i = 0; i < blendOutSetList.GetNumBlendOutSets(); i++)
	{
		blendOutData->m_BlendOutSetList.Add(blendOutSetList.GetBlendOutSet(i));
	}

	OnSelectedBlendOutSetListChanged(blendOutData);
}

void CAnimViewer::OnRecalculateMatch(BlendOutData* blendOutData)
{
	crSkeleton skeleton;
	skeleton.Init(*blendOutData->m_SkeletonData, &blendOutData->m_RagdollTransform);
	blendOutData->m_RagdollFrame->Pose(skeleton);
	skeleton.Update();

	fwMvClipSetId outClipSetId;
	fwMvClipId outClipId;
	blendOutData->m_PoseMatcher.FindBestMatch(skeleton, outClipSetId, outClipId, RC_MATRIX34(blendOutData->m_MatchedTransform), &blendOutData->m_BlendOutSetList.GetFilter());

	blendOutData->m_MatchedItem = blendOutData->m_BlendOutSetList.FindPoseItem(outClipSetId, outClipId, blendOutData->m_MatchedSet);
	if (blendOutData->m_MatchedItem != NULL)
	{
		blendOutData->m_MatchedClipSet = blendOutData->m_MatchedItem->GetClipSet();
		blendOutData->m_MatchedClip = blendOutData->m_MatchedItem->GetClip();
	}

	blendOutData->OnMatchChanged();
}

void CAnimViewer::BlendOutData::OnMatchChanged()
{
	if (animVerifyf(m_MatchedItem != NULL, "CAnimViewer::BlendOutData::OnMatchChanged: Invalid matched blend out item") &&
		animVerifyf(m_MatchedItem->ShouldAddToPointCloud(), "CAnimViewer::BlendOutData::OnMatchChanged: Invalid matched blend out item - should not be added to the point cloud") &&
		animVerifyf(m_MatchedFrame != NULL, "CAnimViewer::BlendOutData::OnMatchChanged: Invalid matched frame"))
	{
		fwClipSet* clipSet = fwClipSetManager::GetClipSet(m_MatchedItem->GetClipSet());
		if (animVerifyf(clipSet != NULL, "CAnimViewer::BlendOutData::OnMatchChanged: Invalid clip set specfied in selected blend out"))
		{
			fwClipDictionaryLoader loader(clipSet->GetClipDictionaryIndex().Get());
			if (animVerifyf(loader.IsLoaded(), "CAnimViewer::BlendOutData::OnMatchChanged: Clip dictionary for clip set for selected blend out not loaded"))
			{
				crClip* clip = clipSet->GetClip(m_MatchedItem->GetClip());
				if (animVerifyf(clip != NULL, "CAnimViewer::BlendOutData::OnMatchChanged: Invalid clip specified in selected blend out"))
				{
					float outTime = 0.0f;
					// Determine the skeleton transform
					if (animVerifyf(m_RagdollFrame != NULL, "CAnimViewer::BlendOutData::OnMatchChanged: Invalid ragdoll frame") &&
						animVerifyf(m_SkeletonData != NULL, "CAnimViewer::BlendOutData::OnMatchChanged: Invalid skeleton data"))
					{
						fwPoseMatcher::Filter filter;
						filter.AddKey(m_MatchedItem->GetClipSet(), m_MatchedItem->GetClip(), 1.0f);

						crSkeleton skeleton;
						skeleton.Init(*m_SkeletonData, &m_RagdollTransform);
						m_RagdollFrame->Pose(skeleton);
						skeleton.Update();

						fwMvClipSetId outClipSetId;
						fwMvClipId outClipId;
						m_PoseMatcher.FindBestMatch(skeleton, outClipSetId, outClipId, outTime, RC_MATRIX34(m_MatchedTransform), &filter);
						animAssertf(outClipSetId == m_MatchedItem->GetClipSet() && outClipId == m_MatchedItem->GetClip(), "CAnimViewer::BlendOutData::OnMatchChanged: Did not find a valid pose match for %s/%s", m_MatchedItem->GetClipSet().GetCStr(), m_MatchedItem->GetClip().GetCStr());
					}

					clip->Composite(*m_MatchedFrame, outTime);
				}
			}
		}
	}
}

void CAnimViewer::BlendOutData::AddWidgets(bkGroup* group)
{
	m_BlendOutGroup = group->AddGroup(m_Title.c_str(), false, "Blend out data");
	if (animVerifyf(m_BlendOutGroup != NULL, "CAnimViewer::BlendOutData::AddWidgets: Invalid blend out group widget"))
	{
		m_BlendOutGroup->AddToggle("Use Low LOD Filter", &m_UseLowLODFilter);
		m_BlendOutGroup->AddButton("Pose Selected Ped Using Currently Drawn Skeleton", datCallback(CFA1(CAnimViewer::OnPoseSelectedPedUsingCurrentlyDrawnSkelecton), (CallbackData)this));
		m_BlendOutGroup->AddButton("Force Selected Ped Into GetUp Task", datCallback(CFA1(CAnimViewer::OnForceSelectedPedIntoGetUpTask), (CallbackData)this));
		m_BlendOutGroup->AddButton("Set Selected Ped Position Using Currently Drawn Skeleton", datCallback(CFA1(CAnimViewer::OnSetSelectedPedPositionUsingCurrentlyDrawnSkeleton), (CallbackData)this));
		m_BlendOutGroup->AddVector("Selected Ped Orientation Offset", &CAnimViewer::m_BlendOutOrientationOffset, -180.0f, 180.0f, 0.01f);
		m_BlendOutGroup->AddVector("Selected Ped Position Offset", &CAnimViewer::m_BlendOutTranslationOffset, -1000.0f, 1000.0f, 0.01f);

		m_BlendOutGroup->AddTitle("Ragdoll");
		m_BlendOutGroup->AddButton("Save Ragdoll Pose", datCallback(CFA1(CAnimViewer::OnSaveRagdollPose), (CallbackData)this));
		m_BlendOutGroup->AddButton("Load Ragdoll Pose", datCallback(CFA1(CAnimViewer::OnLoadRagdollPose), (CallbackData)this));
		m_BlendOutGroup->AddToggle("Draw Ragdoll Frame Skeleton", &m_DrawRagdollFrameSkeleton);
		
		m_BlendOutGroup->AddTitle("Matched");
		m_BlendOutGroup->AddText("Matched Set", &m_MatchedSet, true);
		m_BlendOutGroup->AddText("Matched Clip Set", &m_MatchedClipSet, true);
		m_BlendOutGroup->AddText("Matched Clip", &m_MatchedClip, true);
		m_BlendOutGroup->AddButton("Recalculate Match", datCallback(CFA1(CAnimViewer::OnRecalculateMatch), (CallbackData)this));
		m_BlendOutGroup->AddToggle("Draw Matched Blend Out Item Skeleton", &m_DrawMatchedBlendOutSkeleton);
		
		m_BlendOutGroup->AddTitle("Selected");
		bkCombo* blendOutSetListCombo = m_BlendOutGroup->AddCombo("Blend Out Sets", &m_BlendOutSet, g_NmBlendOutPoseManager.m_sets.GetCount(), NULL, NullCB, "List of all blend out sets", NULL);
		if (animVerifyf(blendOutSetListCombo != NULL, "CAnimViewer::BlendOutData::AddWidgets: Invalid blend out set list combo"))
		{
			for (int i = 0; i < g_NmBlendOutPoseManager.m_sets.GetCount(); i++)
			{
				blendOutSetListCombo->SetString(i, CNmBlendOutSetManager::GetBlendOutSetName((eNmBlendOutSet)*g_NmBlendOutPoseManager.m_sets.GetKey(i)));
			}
		}
		m_BlendOutGroup->AddButton("Add Blend Out Set", datCallback(CFA1(CAnimViewer::OnAddBlendOutSet), (CallbackData)this));
		m_BlendOutGroup->AddButton("Remove Blend Out Set", datCallback(CFA1(CAnimViewer::OnRemoveBlendOutSet), (CallbackData)this));

		OnSelectedBlendOutSetListChanged(this);

		m_BlendOutGroup->AddToggle("Draw Selected Blend Out Item Skeleton", &m_DrawSelectedBlendOutSkeleton);
	}
}

CAnimViewer::BlendOutData* CAnimViewer::CreateNewBlendOutData()
{
	// Delete the oldest entry if the blend out data array is getting out of hand...
	static int maxBlendOutData = 48;
	if (m_BlendOutData.GetNumItems() >= maxBlendOutData)
	{
		atSNode<BlendOutData>* blendOutData = m_BlendOutData.PopHead();
		delete blendOutData;
	}

	atSNode<BlendOutData>* blendOutDataNode = rage_new atSNode<BlendOutData>;
	m_BlendOutData.Append(*blendOutDataNode);

	return &blendOutDataNode->Data;
}

void CAnimViewer::BlendOutCapture(const CDynamicEntity* entity, const CNmBlendOutSetList& blendOutSetList, 
								  const eNmBlendOutSet& matchedSet, CNmBlendOutItem* matchedItem, const char* preTitle)
{
	// Ensure we have an entity and the anim viewer widgets have been enabled
	if (entity != NULL && m_BlendOutDataEnabled)
	{
		BlendOutData* blendOutData = CreateNewBlendOutData();

		if (animVerifyf(blendOutData != NULL, "CAnimViewer::BlendOutCapture: Invalid blend out data"))
		{
			blendOutData->Init(*entity, blendOutSetList, matchedSet, matchedItem, preTitle);

			if (m_BlendOutDataGroup != NULL)
			{
				blendOutData->AddWidgets(m_BlendOutDataGroup);
			}
			else
			{
				OnSelectedBlendOutSetListChanged(blendOutData);
			}
		}
	}
}

// When the selected blend out set list changes we need to refresh the selected blend out list and frame which is then used to pose the skeleton when rendering the selected blend out
void CAnimViewer::OnSelectedBlendOutSetListChanged(BlendOutData* blendOutData)
{
	if (blendOutData->m_BlendOutGroup != NULL)
	{
		// Invalidate the selected blend out list
		blendOutData->m_SelectedBlendOutSet = -1;

		bkCombo* combo = NULL;
		static const u32 comboTitle = ATSTRINGHASH("Selected Blend Out Set List", 0xCC98315E);
		for (bkWidget* child = blendOutData->m_BlendOutGroup->GetChild(); child != NULL; child = child->GetNext())
		{
			if (atStringHash(child->GetTitle()) == comboTitle)
			{
				combo = dynamic_cast<bkCombo*>(child);
				break;
			}
		}

		if (blendOutData->m_BlendOutSetList.GetNumBlendOutSets() > 0)
		{
			// Select a valid blend out
			blendOutData->m_SelectedBlendOutSet = 0;

			// Update or create the combo
			if (combo != NULL)
			{
				combo->UpdateCombo("Selected Blend Out Set List", &blendOutData->m_SelectedBlendOutSet, blendOutData->m_BlendOutSetList.GetNumBlendOutSets(), NULL, datCallback(CFA1(CAnimViewer::OnSelectedBlendOutSetChanged), (CallbackData)blendOutData), "List of blend out sets used for this blend out");
			}
			else
			{
				combo = blendOutData->m_BlendOutGroup->AddCombo("Selected Blend Out Set List", &blendOutData->m_SelectedBlendOutSet, blendOutData->m_BlendOutSetList.GetNumBlendOutSets(), NULL, datCallback(CFA1(CAnimViewer::OnSelectedBlendOutSetChanged), (CallbackData)blendOutData), "List of blend out sets used for this blend out");
			}

			// Ensure that a combo exists and then update the entries
			if (animVerifyf(combo != NULL, "CAnimViewer::OnSelectedBlendOutSetListChanged: Invalid blend out set list combo"))
			{
				for (int i = 0; i < blendOutData->m_BlendOutSetList.GetNumBlendOutSets(); i++)
				{
					combo->SetString(i, CNmBlendOutSetManager::GetBlendOutSetName(blendOutData->m_BlendOutSetList.GetBlendOutSet(i)));
				}
			}
		}
		// If there are no blend out items in the set then remove the combo!
		else if (combo != NULL)
		{
			combo->Destroy();
			combo = NULL;
		}
	}

	OnSelectedBlendOutSetChanged(blendOutData);
}

// When the selected blend out set changes we need to refresh the selected blend out list and frame which is then used to pose the skeleton when rendering the selected blend out
void CAnimViewer::OnSelectedBlendOutSetChanged(BlendOutData* blendOutData)
{
	if (blendOutData->m_BlendOutGroup != NULL)
	{
		// Invalidate the selected blend out
		blendOutData->m_SelectedBlendOut = -1;

		bkCombo* combo = NULL;
		static const u32 comboTitle = ATSTRINGHASH("Selected Blend Out Item", 0xA069463A);
		for (bkWidget* child = blendOutData->m_BlendOutGroup->GetChild(); child != NULL; child = child->GetNext())
		{
			if (atStringHash(child->GetTitle()) == comboTitle)
			{
				combo = dynamic_cast<bkCombo*>(child);
				break;
			}
		}

		CNmBlendOutSet* blendOutSet = NULL;
		if (blendOutData->m_SelectedBlendOutSet < blendOutData->m_BlendOutSetList.GetNumBlendOutSets())
		{
			blendOutSet = CNmBlendOutSetManager::GetBlendOutSet(blendOutData->m_BlendOutSetList.GetBlendOutSet(blendOutData->m_SelectedBlendOutSet));
		}

		if (animVerifyf(blendOutSet != NULL, "CAnimViewer::BlendOutCapture: Invalid blend out set") && blendOutSet->m_items.GetCount() > 0)
		{
			// Select a valid blend out
			blendOutData->m_SelectedBlendOut = 0;

			// Update or create the combo
			if (combo != NULL)
			{
				combo->UpdateCombo("Selected Blend Out Item", &blendOutData->m_SelectedBlendOut, blendOutSet->m_items.GetCount(), NULL, datCallback(CFA1(CAnimViewer::OnSelectedBlendOutChanged), (CallbackData)blendOutData));
			}
			else
			{
				combo = blendOutData->m_BlendOutGroup->AddCombo("Selected Blend Out Item", &blendOutData->m_SelectedBlendOut, blendOutSet->m_items.GetCount(), NULL, datCallback(CFA1(CAnimViewer::OnSelectedBlendOutChanged), (CallbackData)blendOutData));
			}

			// Ensure that a combo exists and then update the entries
			if (animVerifyf(combo != NULL, "CAnimViewer::BlendOutCapture: Invalid blend out set combo"))
			{
				char str[128];
				for (s16 i = 0; i < blendOutSet->m_items.GetCount(); i++)
				{
					if (blendOutSet->m_items[i]->ShouldAddToPointCloud())
					{
						if (blendOutData->m_RagdollFrame != NULL && blendOutData->m_SkeletonData != NULL)
						{
							fwPoseMatcher::Filter filter;
							filter.AddKey(blendOutSet->m_items[i]->GetClipSet(), blendOutSet->m_items[i]->GetClip(), blendOutData->m_BlendOutSetList.GetFilter().PreFilter(fwPoseMatcher::CalcKey(blendOutSet->m_items[i]->GetClipSet(), blendOutSet->m_items[i]->GetClip()), 0.0f));

							crSkeleton skeleton;
							skeleton.Init(*blendOutData->m_SkeletonData, &blendOutData->m_RagdollTransform);
							blendOutData->m_RagdollFrame->Pose(skeleton);
							skeleton.Update();

							fwMvClipSetId outClipSetId;
							fwMvClipId outClipId;
							Matrix34 outTransform;
							float outTime;
							blendOutData->m_PoseMatcher.FindBestMatch(skeleton, outClipSetId, outClipId, outTime, outTransform, &filter);
							animAssertf(outClipSetId == blendOutSet->m_items[i]->GetClipSet() && outClipId == blendOutSet->m_items[i]->GetClip(), "CAnimViewer::BlendOutCapture: Did not find a valid pose match for %s/%s", blendOutSet->m_items[i]->GetClipSet().GetCStr(), blendOutSet->m_items[i]->GetClip().GetCStr());

							formatf(str, sizeof(str), "%s/%s @ %.3fs - %.4f", blendOutSet->m_items[i]->GetClipSet().GetCStr(), blendOutSet->m_items[i]->GetClip().GetCStr(), outTime, filter.GetBestDistance());
						}
						else
						{
							formatf(str, sizeof(str), "%s/%s", blendOutSet->m_items[i]->GetClipSet().GetCStr(), blendOutSet->m_items[i]->GetClip().GetCStr());
						}
					}
					else
					{
						formatf(str, sizeof(str), "invalid pose item");
					}

					combo->SetString(i, str);
				}
			}
		}
		// If there are no blend out items in the set then remove the combo!
		else if (combo != NULL)
		{
			combo->Destroy();
			combo = NULL;
		}
	}

	OnSelectedBlendOutChanged(blendOutData);
}

// When the selected blend out changes we need to refresh the selected frame which is then used to pose the skeleton when rendering the selected blend out
void CAnimViewer::OnSelectedBlendOutChanged(BlendOutData* blendOutData)
{
	if (animVerifyf(blendOutData->m_SelectedFrame != NULL, "CAnimViewer::OnSelectedBlendOutChange: Invalid selected frame") &&
		animVerifyf(blendOutData->m_SelectedBlendOutSet >= 0 && blendOutData->m_SelectedBlendOutSet < blendOutData->m_BlendOutSetList.GetNumBlendOutSets(), "Invalid blend out set selected"))
	{
		CNmBlendOutSet* blendOutSet = CNmBlendOutSetManager::GetBlendOutSet(blendOutData->m_BlendOutSetList.GetBlendOutSet(blendOutData->m_SelectedBlendOutSet));
		if (animVerifyf(blendOutSet != NULL, "CAnimViewer::OnSelectedBlendOutChanged: Invalid blend out set selected") && blendOutData->m_SelectedBlendOut >= 0 && blendOutData->m_SelectedBlendOut < blendOutSet->m_items.GetCount())
		{
			CNmBlendOutItem* blendOutItem = blendOutSet->m_items[blendOutData->m_SelectedBlendOut];
			if(animVerifyf(blendOutItem != NULL, "CAnimViewer::OnSelectedBlendOutChanged: Invalid blend out") && blendOutItem->ShouldAddToPointCloud())
			{
				fwClipSet* clipSet = fwClipSetManager::GetClipSet(blendOutItem->GetClipSet());
				if (animVerifyf(clipSet != NULL, "CAnimViewer::OnSelectedBlendOutChanged: Invalid clip set %s specfied in selected blend out", blendOutItem->GetClipSet().GetCStr()))
				{
					fwClipDictionaryLoader loader(clipSet->GetClipDictionaryIndex().Get());
					if (animVerifyf(loader.IsLoaded(), "CAnimViewer::OnSelectedBlendOutChanged: Clip dictionary %s for clip set %s not loaded", 
						g_ClipDictionaryStore.GetName(clipSet->GetClipDictionaryIndex()), blendOutItem->GetClipSet().GetCStr()))
					{
						crClip* clip = clipSet->GetClip(blendOutItem->GetClip());
						if(animVerifyf(clip != NULL, "CAnimViewer::OnSelectedBlendOutChanged: Invalid clip %s specified in selected blend out", blendOutItem->GetClip().GetCStr()))
						{
							float outTime = 0.0f;
							// Determine the skeleton transform and time
							if (animVerifyf(blendOutData->m_RagdollFrame != NULL, "CAnimViewer::OnSelectedBlendOutChanged: Invalid ragdoll frame") &&
								animVerifyf(blendOutData->m_SkeletonData != NULL, "CAnimViewer::OnSelectedBlendOutChanged: Invalid skeleton data"))
							{
								fwPoseMatcher::Filter filter;
								filter.AddKey(blendOutItem->GetClipSet(), blendOutItem->GetClip(), 1.0f);

								crSkeleton skeleton;
								skeleton.Init(*blendOutData->m_SkeletonData, &blendOutData->m_RagdollTransform);
								blendOutData->m_RagdollFrame->Pose(skeleton);
								skeleton.Update();

								fwMvClipSetId outClipSetId;
								fwMvClipId outClipId;
								blendOutData->m_PoseMatcher.FindBestMatch(skeleton, outClipSetId, outClipId, outTime, RC_MATRIX34(blendOutData->m_SelectedTransform), &filter);
								animAssertf(outClipSetId == blendOutItem->GetClipSet() && outClipId == blendOutItem->GetClip(), "CAnimViewer::OnSelectedBlendOutChanged: Did not find a valid pose match for %s/%s", blendOutItem->GetClipSet().GetCStr(), blendOutItem->GetClip().GetCStr());
							}

							clip->Composite(*blendOutData->m_SelectedFrame, outTime);
						}
					}
				}
			}
		}
	}
}

void CAnimViewer::ClearBlendOutData()
{
	atSNode<BlendOutData>* node;
	while ((node = m_BlendOutData.PopHead()) != NULL)
	{
		delete node;
	}
}

void CAnimViewer::PositionPedRelativeToVehicleSeatCB()
{
	CVehicle* pVehicle = CVehicleDebug::GetFocusVehicle();
	CPed* pPed = CPedDebugVisualiserMenu::GetFocusPed();

	if (taskVerifyf(pPed && pVehicle, "Select a ped and a vehicle"))
	{
		pPed->SetUseExtractedZ(true);
		pPed->DetachFromParent(DETACH_FLAG_IGNORE_SAFE_POSITION_CHECK|DETACH_FLAG_SKIP_CURRENT_POSITION_CHECK);
		pPed->DisableCollision();

		// Render text at the entry point
		Vector3 vSeatPos(Vector3::ZeroType);
		Quaternion qSeatOrientation(0.0f,0.0f,0.0f,1.0f);
		CModelSeatInfo::CalculateSeatSituation(pVehicle, vSeatPos, qSeatOrientation, ms_iSeat);

		Vector3 vVehicleDirection = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetB());
		const float fVehicleHeading = rage::Atan2f(-vVehicleDirection.x, vVehicleDirection.y);
		Vector3 vSecondPedsOffset(ms_fXRelativeOffset, ms_fYRelativeOffset, ms_fZRelativeOffset);
		float fTotalHeadingRotation = 0.0f;

		if (ms_bComputeOffsetFromAnim)
		{
			if (taskVerifyf((ms_aFirstPedDicts.GetCount() > 0) && (ms_szFirstPedsAnimNames.GetCount() > 0), "Need to set anims on first ped"))
			{
				s32 animDictIndex = fwAnimManager::FindSlot(ms_aFirstPedDicts[0]->GetName()).Get();
				u32 animHashKey = atStringHash(ms_szFirstPedsAnimNames[0]);

				CClipRequestHelper animRequest;
				if (animRequest.RequestClipsByIndex(animDictIndex))
				{
					const crClip* pClip = fwAnimManager::GetClipIfExistsByDictIndex(animDictIndex, animHashKey);
					if(Verifyf(pClip, "Animation does not exist"))
					{
						//Now that we can identify the anim, get the mover track displacement.
						// WARNING: Original code converted as found, but start and end seemed reversed?
						Vector3 vOffset = fwAnimHelpers::GetMoverTrackTranslationDiff(*pClip, 1.f, 0.f);
						animDisplayf("Anim offset from end to beginning : (%.2f,%.2f,%.2f)", vOffset.x, vOffset.y, vOffset.z);
						Quaternion qRotStart = fwAnimHelpers::GetMoverTrackRotationDiff(*pClip, 0.f, 1.f);
						Vector3 vRotationEulers(Vector3::ZeroType);
						qRotStart.ToEulers(vRotationEulers, "xyz");
						float fTotalHeadingRotation = vRotationEulers.z;
						animDisplayf("Anim rotation from end to beginning : (%.2f)", fTotalHeadingRotation);
						vSecondPedsOffset = vOffset;
					}
				}
			}
		}

		vSecondPedsOffset.RotateZ(fVehicleHeading);

		Vector3 vSecondPedPosition = vSeatPos + vSecondPedsOffset;
		pPed->SetPosition(vSecondPedPosition);
		const float fSecondPedsHeading = fwAngle::LimitRadianAngle(fVehicleHeading + fTotalHeadingRotation + ms_fRelativeHeading);
		pPed->SetHeading(fSecondPedsHeading);
		pPed->SetDesiredHeading(fSecondPedsHeading);
	}
}

void CAnimViewer::PositionPedsCB()
{
	CEntity* pFocusEntity1 = CDebugScene::FocusEntities_Get(0);
	if (pFocusEntity1 && pFocusEntity1->GetIsTypePed())
	{
		CPed* pFirstPed = static_cast<CPed*>(pFocusEntity1);
		pFirstPed->SetUseExtractedZ(true);
		pFirstPed->DetachFromParent(DETACH_FLAG_IGNORE_SAFE_POSITION_CHECK|DETACH_FLAG_SKIP_CURRENT_POSITION_CHECK);
		pFirstPed->DisableCollision();

		CEntity* pFocusEntity2 = CDebugScene::FocusEntities_Get(1);
		if (pFocusEntity2 && pFocusEntity2->GetIsTypePed())
		{
			CPed* pSecondPed = static_cast<CPed*>(pFocusEntity2);
			
			pSecondPed->SetUseExtractedZ(true);
			pSecondPed->DetachFromParent(DETACH_FLAG_IGNORE_SAFE_POSITION_CHECK|DETACH_FLAG_SKIP_CURRENT_POSITION_CHECK);
			pSecondPed->DisableCollision();

			Vector3 vFirstPedPosition = VEC3V_TO_VECTOR3(pFirstPed->GetTransform().GetPosition());
			vFirstPedPosition.z += ms_fExtraZOffsetForBothPeds;
			pFirstPed->SetPosition(vFirstPedPosition);

			const float fFirstPedsHeading = pFirstPed->GetCurrentHeading();
			Vector3 vSecondPedsOffset(ms_fXRelativeOffset, ms_fYRelativeOffset, ms_fZRelativeOffset);
			vSecondPedsOffset.RotateZ(fFirstPedsHeading);

			Vector3 vSecondPedPosition = vFirstPedPosition + vSecondPedsOffset;
			pSecondPed->SetPosition(vSecondPedPosition);
			const float fSecondPedsHeading = fwAngle::LimitRadianAngle(fFirstPedsHeading + ms_fRelativeHeading);
			pSecondPed->SetHeading(fSecondPedsHeading);
			pSecondPed->SetDesiredHeading(fSecondPedsHeading);
		}
	}
}

void CAnimViewer::ResetPedsCB()
{
	CEntity* pFocusEntity1 = CDebugScene::FocusEntities_Get(0);
	if (pFocusEntity1 && pFocusEntity1->GetIsTypePed())
	{
		CPed* pFirstPed = static_cast<CPed*>(pFocusEntity1);
		pFirstPed->SetUseExtractedZ(false);
		pFirstPed->GetPedIntelligence()->FlushImmediately(true);
		pFirstPed->EnableCollision();

		CEntity* pFocusEntity2 = CDebugScene::FocusEntities_Get(1);
		if (pFocusEntity2 && pFocusEntity2->GetIsTypePed())
		{
			CPed* pSecondPed = static_cast<CPed*>(pFocusEntity2);
			pSecondPed->SetUseExtractedZ(false);
			pSecondPed->GetPedIntelligence()->FlushImmediately(true);

			pSecondPed->EnableCollision();
		}
	}
}

void CAnimViewer::AddAnimToFirstPedsListCB()
{
	int index;
	ms_aFirstPedDicts.PushAndGrow(CDebugClipDictionary::FindClipDictionary(atStringHash(CAnimViewer::m_animSelector.GetSelectedClipDictionary()), index));
	ms_szFirstPedsAnimNames.PushAndGrow(CAnimViewer::m_animSelector.GetSelectedClipName());
}

void CAnimViewer::AddAnimToSecondPedsListCB()
{
	int index;
	ms_aSecondPedDicts.PushAndGrow(CDebugClipDictionary::FindClipDictionary(atStringHash(CAnimViewer::m_animSelector.GetSelectedClipDictionary()), index));
	ms_szSecondPedsAnimNames.PushAndGrow(CAnimViewer::m_animSelector.GetSelectedClipName());
}

void CAnimViewer::ClearFirstPedsAnimsCB()
{
	ms_aFirstPedDicts.clear();
	ms_szFirstPedsAnimNames.clear();
}

void CAnimViewer::ClearSecondPedsAnimsCB()
{
	ms_aSecondPedDicts.clear();
	ms_szSecondPedsAnimNames.clear();
}

void CAnimViewer::PlayAnimsCB()
{
	CEntity* pFocusEntity1 = CDebugScene::FocusEntities_Get(0);
	if (pFocusEntity1 && pFocusEntity1->GetIsTypePed())
	{
		CPed* pFirstPed = static_cast<CPed*>(pFocusEntity1);

		if (taskVerifyf((ms_aFirstPedDicts.GetCount() > 0) && (ms_szFirstPedsAnimNames.GetCount() > 0), "Need to set anims on first ped"))
		{
			CTaskSequenceList* pFirstList = rage_new CTaskSequenceList();
			for (s32 i=0; i<ms_aFirstPedDicts.GetCount(); i++)
			{
				pFirstList->AddTask(rage_new CTaskRunNamedClip(ms_aFirstPedDicts[i]->GetName(), ms_szFirstPedsAnimNames[i], AP_HIGH, APF_ISFINISHAUTOREMOVE, BONEMASK_ALL, INSTANT_BLEND_IN_DELTA, -1, CTaskClip::ATF_RUN_IN_SEQUENCE | CTaskClip::ATF_TURN_OFF_COLLISION | CTaskClip::ATF_IGNORE_GRAVITY, 0.0f));
			}

			pFirstPed->GetPedIntelligence()->AddTaskAtPriority(rage_new CTaskUseSequence(*pFirstList), PED_TASK_PRIORITY_PRIMARY, true);
		}
	}

	CEntity* pFocusEntity2 = CDebugScene::FocusEntities_Get(1);
	if (pFocusEntity2 && pFocusEntity2->GetIsTypePed())
	{
		CPed* pSecondPed = static_cast<CPed*>(pFocusEntity2);

		if (taskVerifyf((ms_aSecondPedDicts.GetCount() > 0) && (ms_szSecondPedsAnimNames.GetCount() > 0), "Need to set anims on second ped"))
		{
			CTaskSequenceList* pSecondList = rage_new CTaskSequenceList();
			for (s32 i=0; i<ms_aSecondPedDicts.GetCount(); i++)
			{
				pSecondList->AddTask(rage_new CTaskRunNamedClip(ms_aSecondPedDicts[i]->GetName(), ms_szSecondPedsAnimNames[i], AP_HIGH, APF_ISFINISHAUTOREMOVE, BONEMASK_ALL, INSTANT_BLEND_IN_DELTA, -1, CTaskClip::ATF_RUN_IN_SEQUENCE | CTaskClip::ATF_TURN_OFF_COLLISION | CTaskClip::ATF_IGNORE_GRAVITY, 0.0f));
			}

			pSecondPed->GetPedIntelligence()->AddTaskAtPriority(rage_new CTaskUseSequence(*pSecondList), PED_TASK_PRIORITY_PRIMARY, true);
		}
	}
}

void CAnimViewer::GoToStartAnim()
{
	if (m_pDynamicEntity->GetIsTypeObject())
	{
	}
	else
	{
		const crClip* pClip = m_animSelector.GetSelectedClip();

		atHashString animHash;
		if(pClip)
		{
			animHash = pClip->GetName();
		}

		if(animHash.GetHash() != m_AnimHash.GetHash())
		{
			m_MoveNetworkHelper.SetClip(pClip, ms_ClipId);

			m_MoveNetworkHelper.SetFloat(ms_PhaseId, m_Phase);
			m_MoveNetworkHelper.SetBoolean(ms_LoopedId, true);

			u32 frames = pClip ? (u32)pClip->GetNum30Frames() : 2;
			m_pFrameSlider->SetRange(0.0f, (float)frames-1.0f);

			m_AnimHash = animHash;
		}

		m_OldPhase = m_Phase = 0.0f;
		m_MoveNetworkHelper.SetFloat(ms_PhaseId, m_Phase);
	}
}

void CAnimViewer::StartBlendAnim()
{
	if (m_pDynamicEntity->GetIsTypeObject())
	{
	}
	else
	{
		const crClip* pClip = m_blendAnimSelector.GetSelectedClip();

		atHashString animHash;
		if(pClip)
		{
			animHash = pClip->GetName();
		}

		if(animHash.GetHash() != m_BlendAnimHash.GetHash())
		{
			m_MoveNetworkHelper.SetClip(pClip, ms_BlendClipId);
			m_MoveNetworkHelper.SetFloat(ms_BlendAnimPhaseId, m_AnimBlendPhase);	
			m_BlendAnimHash = animHash;					
		}

		m_OldAnimBlendPhase = m_AnimBlendPhase = 0.0f;
		m_MoveNetworkHelper.SetFloat(ms_PhaseId, m_AnimBlendPhase);
	}
}

void CAnimViewer::StepForwardAnim()
{
	if (m_pDynamicEntity->GetIsTypeObject())
	{
	}
	else
	{
		const crClip* pClip = m_animSelector.GetSelectedClip();

		atHashString animHash;
		if(pClip)
		{
			animHash = pClip->GetName();
		}

		if(animHash.GetHash() != m_AnimHash.GetHash())
		{
			m_MoveNetworkHelper.SetClip(pClip, ms_ClipId);

			m_MoveNetworkHelper.SetFloat(ms_PhaseId, m_Phase);
			m_MoveNetworkHelper.SetBoolean(ms_LoopedId, true);

			u32 frames = pClip ? (u32)pClip->GetNum30Frames() : 2;
			m_pFrameSlider->SetRange(0.0f, (float)frames-1.0f);

			m_AnimHash = animHash;
		}

		m_Rate = 0.0f;
		m_MoveNetworkHelper.SetFloat(ms_RateId, m_Rate);

		m_OldPhase = m_Phase = m_MoveNetworkHelper.GetFloat(ms_PhaseOutId);
		u32 frames = pClip ? (u32)pClip->GetNum30Frames() : 2;
		m_Phase = floorf(m_Phase * frames) / frames;
		m_Phase += 1.0f / (frames - 1.0f);
		while(m_Phase > 1.0f) { m_Phase -= 1.0f; }
 		m_MoveNetworkHelper.SetFloat(ms_PhaseId, m_Phase);
	}
}

void CAnimViewer::PlayForwardsAnim()
{
	if (m_pDynamicEntity->GetIsTypeObject())
	{
		// use an animation task on objects
		CObject * pObject = static_cast<CObject*>(m_pDynamicEntity.Get());
		pObject->SetTask(rage_new CTaskRunNamedClip( m_animSelector.GetSelectedClipDictionary(), m_animSelector.GetSelectedClipName(), AP_HIGH, APF_ISLOOPED, BONEMASK_ALL, INSTANT_BLEND_IN_DELTA, -1, CTaskClip::ATF_DRIVE_TO_POSE, 0.0f ) );
	}
	else
	{
		const crClip* pClip = m_animSelector.GetSelectedClip();

		atHashString animHash;
		if(pClip)
		{
			animHash = pClip->GetName();
		}

		if(animHash.GetHash() != m_AnimHash.GetHash())
		{
			m_MoveNetworkHelper.SetClip(pClip, ms_ClipId);

			m_MoveNetworkHelper.SetFloat(ms_PhaseId, m_Phase);
			m_MoveNetworkHelper.SetBoolean(ms_LoopedId, true);

			u32 frames = pClip ? (u32)pClip->GetNum30Frames() : 2;
			m_pFrameSlider->SetRange(0.0f, (float)frames-1.0f);

			m_AnimHash = animHash;
		}

		m_Rate = 1.0f;
		m_MoveNetworkHelper.SetFloat(ms_RateId, m_Rate);
	}
}

void CAnimViewer::PauseAnim()
{
	if (m_pDynamicEntity->GetIsTypeObject())
	{
	}
	else
	{
		const crClip* pClip = m_animSelector.GetSelectedClip();

		atHashString animHash;
		if(pClip)
		{
			animHash = pClip->GetName();
		}

		if(animHash.GetHash() != m_AnimHash.GetHash())
		{
			m_MoveNetworkHelper.SetClip(pClip, ms_ClipId);

			m_MoveNetworkHelper.SetFloat(ms_PhaseId, m_Phase);
			m_MoveNetworkHelper.SetBoolean(ms_LoopedId, true);

			u32 frames = pClip ? (u32)pClip->GetNum30Frames() : 2;
			m_pFrameSlider->SetRange(0.0f, (float)frames-1.0f);

			m_AnimHash = animHash;
		}

		m_Rate = 0.0f;
		m_MoveNetworkHelper.SetFloat(ms_RateId, m_Rate);
	}
}

void CAnimViewer::PlayBackwardsAnim()
{
	if (m_pDynamicEntity->GetIsTypeObject())
	{
	}
	else
	{
		const crClip* pClip = m_animSelector.GetSelectedClip();

		atHashString animHash;
		if(pClip)
		{
			animHash = pClip->GetName();
		}

		if(animHash.GetHash() != m_AnimHash.GetHash())
		{
			m_MoveNetworkHelper.SetClip(pClip, ms_ClipId);

			m_MoveNetworkHelper.SetFloat(ms_PhaseId, m_Phase);
			m_MoveNetworkHelper.SetBoolean(ms_LoopedId, true);

			u32 frames = pClip ? (u32)pClip->GetNum30Frames() : 2;
			m_pFrameSlider->SetRange(0.0f, (float)frames-1.0f);

			m_AnimHash = animHash;
		}

		m_Rate = -1.0f;
		m_MoveNetworkHelper.SetFloat(ms_RateId, m_Rate);
	}
}

void CAnimViewer::StepBackwardAnim()
{
	if (m_pDynamicEntity->GetIsTypeObject())
	{
	}
	else
	{
		const crClip* pClip = m_animSelector.GetSelectedClip();

		atHashString animHash;
		if(pClip)
		{
			animHash = pClip->GetName();
		}

		if(animHash.GetHash() != m_AnimHash.GetHash())
		{
			m_MoveNetworkHelper.SetClip(pClip, ms_ClipId);

			m_MoveNetworkHelper.SetFloat(ms_PhaseId, m_Phase);
			m_MoveNetworkHelper.SetBoolean(ms_LoopedId, true);

			u32 frames = pClip ? (u32)pClip->GetNum30Frames() : 2;
			m_pFrameSlider->SetRange(0.0f, (float)frames-1.0f);

			m_AnimHash = animHash;
		}

		m_Rate = 0.0f;
		m_MoveNetworkHelper.SetFloat(ms_RateId, m_Rate);

		m_OldPhase = m_Phase = m_MoveNetworkHelper.GetFloat(ms_PhaseOutId);
		u32 frames = pClip ? (u32)pClip->GetNum30Frames() : 2;
		m_Phase = ceilf(m_Phase * frames) / frames;
		m_Phase -= 1.0f / (frames - 1.0f);
		while(m_Phase < 0.0f) { m_Phase += 1.0f; }
		m_MoveNetworkHelper.SetFloat(ms_PhaseId, m_Phase);
	}
}

void CAnimViewer::GoToEndAnim()
{
	if (m_pDynamicEntity->GetIsTypeObject())
	{
	}
	else
	{
		const crClip* pClip = m_animSelector.GetSelectedClip();

		atHashString animHash;
		if(pClip)
		{
			animHash = pClip->GetName();
		}

		if(animHash.GetHash() != m_AnimHash.GetHash())
		{
			m_MoveNetworkHelper.SetClip(pClip, ms_ClipId);

			m_MoveNetworkHelper.SetFloat(ms_PhaseId, m_Phase);
			m_MoveNetworkHelper.SetBoolean(ms_LoopedId, true);

			u32 frames = pClip ? (u32)pClip->GetNum30Frames() : 2;
			m_pFrameSlider->SetRange(0.0f, (float)frames-1.0f);

			m_AnimHash = animHash;
		}

		m_OldPhase = m_Phase = 1.0f;
		m_MoveNetworkHelper.SetFloat(ms_PhaseId, m_Phase);
	}
}

void CAnimViewer::StopAnim()
{
	if (m_pDynamicEntity->GetIsTypeObject())
	{
		CObject * pObject = static_cast<CObject*>(m_pDynamicEntity.Get());
		pObject->SetTask(NULL);
	}
	else
	{
		m_MoveNetworkHelper.SetClip(NULL, ms_ClipId ); 
		m_OldPhase = m_Phase = 0.0f;
		m_AnimHash=""; 
	}
}

void CAnimViewer::ReloadAnim()
{
	if(m_animSelector.GetClipContext() == kClipContextLocalFile && m_animSelector.HasSelection())
	{
		bool bWasPlaying = false;
		float phase = 0.0f;
		float rate = 0.0f;
		if (m_pDynamicEntity->GetIsTypeObject())
		{
			CObject *pObject = static_cast< CObject * >(m_pDynamicEntity.Get());
			if(pObject)
			{
				CTask *pTask = pObject->GetTask();
				bWasPlaying = pTask && pTask->GetTaskType() == CTaskTypes::TASK_RUN_NAMED_CLIP;
			}
		}
		else
		{
			bWasPlaying = m_MoveNetworkHelper.IsNetworkActive();
		}

		if(bWasPlaying)
		{
			phase = m_Phase;
			rate = m_Rate;
			StopAnim();
		}

		m_animSelector.ReloadLocalFileClip();

		if(bWasPlaying)
		{
			if(rate >= 0.0f)
			{
				PlayForwardsAnim();
			}
			else
			{
				PlayBackwardsAnim();
			}

			if (!m_pDynamicEntity->GetIsTypeObject())
			{
				m_OldPhase = m_Phase = phase;
				m_MoveNetworkHelper.SetFloat(ms_PhaseId, m_Phase); 
				m_Rate = rate;
				m_MoveNetworkHelper.SetFloat(ms_RateId, m_Rate);
			}
		}
	}
}

void CAnimViewer::SetPhase()
{
	m_MoveNetworkHelper.SetFloat(ms_PhaseId, m_Phase); 

	if(m_OldPhase != m_Phase)
	{
		const crClip *pClip = m_animSelector.GetSelectedClip();
		if(pClip)
		{
			Matrix34 mMover(Matrix34::IdentityType);
			if(m_Phase > m_OldPhase)
			{
				fwAnimHelpers::GetMoverTrackMatrixDelta(*pClip, m_OldPhase, m_Phase, mMover);
			}
			else
			{
				fwAnimHelpers::GetMoverTrackMatrixDelta(*pClip, m_Phase, m_OldPhase, mMover);

				mMover.Inverse();
			}

			Matrix34 mCurrent = MAT34V_TO_MATRIX34(m_pDynamicEntity->GetMatrix());

			mCurrent.DotFromLeft(mMover);

			m_pDynamicEntity->SetMatrix(mCurrent);

			m_OldPhase = m_Phase;
		}
	}
}

void CAnimViewer::SetBlendPhase()
{
	m_MoveNetworkHelper.SetFloat(ms_BlendPhaseId, m_BlendPhase); 
}

void CAnimViewer::SetAnimBlendPhase()
{
	m_MoveNetworkHelper.SetFloat(ms_BlendAnimPhaseId, m_AnimBlendPhase); 
}

void CAnimViewer::SetFrame()
{
	const crClip* pClip = m_animSelector.GetSelectedClip(); 
	if(pClip)
	{
		atHashString animhash = pClip->GetName();
		if(animhash.GetHash() == m_AnimHash.GetHash()) 
		{
			m_Phase = pClip->Convert30FrameToPhase(m_Frame); 

			SetPhase();
		}
	}

}

void CAnimViewer::SetRate()
{
	const crClip* pClip = m_animSelector.GetSelectedClip(); 

	if(pClip)
	{
		atHashString animHash = pClip->GetName();

		if(animHash.GetHash() == m_AnimHash.GetHash()) 
		{
			m_MoveNetworkHelper.SetFloat(ms_RateId, m_Rate);
		}
	}
}

void CAnimViewer::UpdatePhase()
{
	if(m_MoveNetworkHelper.IsNetworkActive())
	{
		if(m_pDynamicEntity && m_pDynamicEntity->GetIsTypeObject() && m_pDynamicEntity->GetFragInst())
		{
			m_pDynamicEntity->GetAnimDirector()->SetUseCurrentSkeletonForActivePose(true);
		}

		m_OldPhase = m_Phase;
		m_Phase = m_MoveNetworkHelper.GetFloat(ms_PhaseOutId); 

		const crClip* pClip = m_animSelector.GetSelectedClip(); 
		
		if(pClip)
		{
			atHashString animhash = pClip->GetName();

			if(animhash.GetHash() == m_AnimHash.GetHash()) 
			{
				float frame  = pClip->ConvertPhaseTo30Frame(m_Phase); 
				m_Frame = frame; 
			}
		}

		
	}
}

crClip* CAnimViewer::GetSelectedClip()
{
	if (m_MoveNetworkHelper.IsNetworkActive())
	{
		return m_animSelector.GetSelectedClip();
	}

	return NULL;
}

float CAnimViewer::GetPhase()
{
	if (m_MoveNetworkHelper.IsNetworkActive())
	{
		return m_Phase;
	}

	return 0.0f;
}

bool CAnimViewer::EnumeratePm(const crpmParameterizedMotionData *pmData, u32, void* data)
{
	bool rv = false;
	if (pmData)
	{
		atVector<atString> *p_pmNames = (atVector<atString>*)data;
		const char* p_fullyQualifiedName = pmData->GetName();

		// Remove the path name leaving the filename and the extension
		const char* fileNameAndExtension = p_fullyQualifiedName;
		fileNameAndExtension = ASSET.FileName(p_fullyQualifiedName);

		// Remove the extension leaving just the filename
		char fileName[256];
		ASSET.RemoveExtensionFromPath(fileName, 256, fileNameAndExtension);

		// Add the file name to the array of file names
		p_pmNames->push_back(atString(&fileName[0]));
		rv = true;
	}

	// Add the dictionary name to the array of dictionary names
	//m_animationDictionaryNames.Grow() = p_dictionaryName;
	//Displayf("%s %s\n", p_dictionaryName, m_animationNameArrays.back().c_str());
	return rv;
}

void CAnimViewer::Reset()
{
	if(!m_pDynamicEntity)
	{
		m_pDynamicEntity = FindPlayerPed();
	}
}

void CAnimViewer::ToggleLockPosition()
{
	if (m_pDynamicEntity)
	{
		if (m_bLockPosition)
		{
			m_startPosition = VEC3V_TO_VECTOR3(m_pDynamicEntity->GetTransform().GetPosition());
		}
	}
}

void CAnimViewer::ToggleUninterruptableTask()
{
	USE_MEMBUCKET(MEMBUCKET_ANIMATION);

	if(!m_pDynamicEntity)
	{
		m_pDynamicEntity = FindPlayerPed();
	}

	if (m_pDynamicEntity) //check this again in case we failed to find the dynamic entity
	{
		if (m_pDynamicEntity->GetIsTypePed())
		{
			CPed* pPed = static_cast<CPed*>(m_pDynamicEntity.Get());
			if(m_unInterruptableTask)
			{
				// Add an interruptible task to the player
				// This overrides the default task CTaskPlayerOnFoot which keep starting idle animations
				// It also stops other task from starting
				pPed->GetPedIntelligence()->AddTaskDefault(rage_new CTaskUninterruptable());
			}
			else
			{
				if(pPed->IsPlayer())
				{
					pPed->GetPedIntelligence()->AddTaskDefault(rage_new CTaskPlayerOnFoot());
				}
				else
				{
					pPed->GetPedIntelligence()->AddTaskDefault(rage_new CTaskDoNothing(-1));
				}
			}
		}
	}
	
}

void CAnimViewer::ToggleExtractZ()
{
	if(!m_pDynamicEntity)
		return;

	if (m_pDynamicEntity->GetIsTypePed())
	{
		CPed* pPed = static_cast<CPed*>(m_pDynamicEntity.Get());
		pPed->SetUseExtractedZ(m_extractZ);
	}
}

#if __DEV

//renders the loaded point cloud for the currently selected anim and anim group
void CAnimViewer::RenderPointCloud()
{
	
	//fist get the 
	Matrix34 rootMatrix;

	if (m_renderPointCloudAtSkelParent)
	{
		rootMatrix.Set(RCC_MATRIX34(*m_pDynamicEntity->GetSkeleton()->GetParentMtx()));
	}
	else
	{
		rootMatrix.Identity();
	}

	if (m_renderLastMatchedPose)
	{
		fwPoseMatcher::DebugRenderPointCloud(&fwPoseMatcher::s_LastMatchedPose,rootMatrix, m_pointCloudBoneSize, Color_red2);
		grcDebugDraw::Text(rootMatrix.d,Color_red2, 0, 0, fwPoseMatcher::s_LastMatchedPoseName?fwPoseMatcher::s_LastMatchedPoseName:"",true);
	}

	if (m_renderPointCloud)
	{
		const char *animDictName = m_animSelector.GetSelectedClipDictionary();
		const char *animName = m_animSelector.GetSelectedClipName();

		// Anim dictname being used as a clipset name (makes no sense)???
		fwMvClipSetId clipSetId(animDictName);
		fwClipSet *clipSet = fwClipSetManager::GetClipSet(clipSetId);
		if (!clipSet)
		{
			return;
		}

		fwMvClipId clipId = fwMvClipId(animName);
		fwClipItem *clipItem = clipSet->GetClipItem(clipId);
		if (!clipItem)
		{
			return;
		}

		m_pDynamicEntity->GetAnimDirector()->GetComponent<fwAnimDirectorComponentRagDoll>()->GetPoseMatcher().DebugRenderPointCloud(clipSetId, clipId, rootMatrix, m_pointCloudBoneSize);
	}
}

#endif //__DEV

void CAnimViewer::ToggleRenderPeds()
{
	animWarningf("NOTE! CPed::PreRender() will not be called while ped skin rendering is off!");

	// this would hide the ped in all phase including wireframe
	//gVpMan.ToggleVisibilityFlagForAllPhases( VIS_ENTITY_MASK_PED );

	// .. but this only hides the ped in non-wireframe phases
	CPedDrawHandler::SetEnableRendering(m_renderSkin);
}

void CAnimViewer::ToggleBoneRotationOverride()
{
	if(m_pDynamicEntity)
	{
		if(!ms_bOverrideBoneRotation)
		{
			Vector3 bindRotation;
			MAT34V_TO_MATRIX34(m_pDynamicEntity->GetSkeletonData().GetDefaultTransform(m_boneIndex)).ToEulersXYZ(bindRotation);

			// Set the bone local rotation to the bind + override
			RC_MATRIX34(m_pDynamicEntity->GetSkeleton()->GetLocalMtx(m_boneIndex)).FromEulersXYZ(bindRotation);
		}
	}
}

void CAnimViewer::ToggleBoneTranslationOverride()
{
	if(m_pDynamicEntity)
	{
		if(!ms_bOverrideBoneTranslation)
		{
			Vector3 bindTranslation = VEC3V_TO_VECTOR3(m_pDynamicEntity->GetSkeletonData().GetDefaultTransform(m_boneIndex).d());

			// Set the bone local translation to the bind + override
			RC_MATRIX34(m_pDynamicEntity->GetSkeleton()->GetLocalMtx(m_boneIndex)).d.Set(bindTranslation);
		}
	}
}


void CAnimViewer::ToggleBoneScaleOverride()
{
	if(m_pDynamicEntity)
	{
		if(!ms_bOverrideBoneScale)
		{
			Vec3V bindScale = m_pDynamicEntity->GetSkeletonData().GetBoneData(m_boneIndex)->GetDefaultScale();

			// Set the bone local translation to the bind + override
			Mat34V& mtx = m_pDynamicEntity->GetSkeleton()->GetLocalMtx(m_boneIndex);
			Scale3x3(mtx, bindScale, mtx);
		}
	}
}

void CAnimViewer::SelectAnim()
{
}

void CAnimViewer::SetFocusEntityFromIndex()
{
	CPhysical* pPhys = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(m_entityScriptIndex, 0);

	if (pPhys)
	{
		SetSelectedEntity(pPhys);
	}
}

void CAnimViewer::FindNearestAnimatedBuildingToPlayer()
{
	CPed* pPed = FindPlayerPed();

	Vec3V vSearchCoords = pPed->GetMatrix().d();

	CDynamicEntity* pEntity = NULL;
	float minDistance = MINMAX_MAX_FLOAT_VAL;

	CAnimatedBuilding::Pool *BuildingPool = CAnimatedBuilding::GetPool();	//	This used to be static. Not sure if that was necessary.
	int i=BuildingPool->GetSize();
	while (i--)
	{
		CAnimatedBuilding *pBuilding = BuildingPool->GetSlot(i);	//	This used to be static. Not sure if that was necessary.
		if(pBuilding && pBuilding->GetIsTypeAnimatedBuilding() && pBuilding->GetIsVisible() && pBuilding->GetDrawable())
		{
			float Dist = VEC3V_TO_VECTOR3(vSearchCoords-pBuilding->GetMatrix().d()).Mag();

			if (Dist < minDistance)
			{
				pEntity = static_cast<CDynamicEntity*>(pBuilding);
			}
		}
	}

	if (pEntity!=NULL)
	{
		SetSelectedEntity(pEntity);
	}
}

void CAnimViewer::UpdateBoneLimits()
{
	if(!m_pDynamicEntity)
	{
		m_pDynamicEntity = FindPlayerPed();
	}

	//// Set the rotation min and max for the given bone index
	//crBoneData* boneData = const_cast<crBoneData*>(m_pDynamicEntity->GetSkeletonData().GetBoneData(m_boneIndex));
	//boneData->SetRotationMin(m_rotationMin);
	//boneData->SetRotationMax(m_rotationMax);

	// fixme JA

}

void CAnimViewer::SelectBone()
{
	if(!m_pDynamicEntity)
	{
		return;
	}

	// Keep the bone index in range
	int numberOfBones = m_pDynamicEntity->GetSkeletonData().GetNumBones();
	numberOfBones--;
	if (m_boneIndex>numberOfBones)
	{
		m_boneIndex = 0;
	}
	if (m_boneIndex<0)
	{
		m_boneIndex = numberOfBones;
	}
}

static void ClearDofWidgets(bkGroup& grp, CAnimViewer::SpringDof& widgets)
{
	if (widgets.m_pLabel)
	{
		grp.Remove(*widgets.m_pLabel); 
		widgets.m_pLabel=NULL;
	}
	if (widgets.m_pStrength)
	{
		grp.Remove(*widgets.m_pStrength); 
		widgets.m_pStrength=NULL;
	}
	if (widgets.m_pDamping)
	{
		grp.Remove(*widgets.m_pDamping); 
		widgets.m_pDamping=NULL;
	}
	if (widgets.m_pMinConstraint)
	{
		grp.Remove(*widgets.m_pMinConstraint); 
		widgets.m_pMinConstraint=NULL;
	}
	if (widgets.m_pMaxConstraint)
	{
	grp.Remove(*widgets.m_pMaxConstraint);
		widgets.m_pMaxConstraint=NULL;
	}
	if (widgets.m_pSeparator)
	{	
		grp.Remove(*widgets.m_pSeparator); 
		widgets.m_pSeparator=NULL;
	}
}

static void PopulateDofWidgets(bkGroup& grp, CAnimViewer::SpringDof& widgets, const char* label, crMotionDescription::Dof3& dof)
{
	static const float min = -10000.F, max = 10000.F, delta = 0.05F;
	widgets.m_pLabel = grp.AddTitle(label);
	widgets.m_pStrength = grp.AddVector("Strength", &dof.m_Strength, min, max, delta);
	widgets.m_pDamping = grp.AddVector("Damping", &dof.m_Damping, min, max, delta);
	widgets.m_pMinConstraint = grp.AddVector("Min Constraint", &dof.m_MinConstraint, min, max, delta);
	widgets.m_pMaxConstraint = grp.AddVector("Max Constraint", &dof.m_MaxConstraint, min, max, delta);
	widgets.m_pSeparator = grp.AddSeparator();
}

void CAnimViewer::PopulateSpringWidgets()
{
	if (m_pDynamicEntity && m_pDynamicEntity->GetCreature())
	{
		crCreature& creature = *m_pDynamicEntity->GetCreature();
		crCreatureComponentPhysical* physicalComponent = creature.FindComponent<crCreatureComponentPhysical>();
		if (physicalComponent != NULL)
		{
			crCreatureComponentPhysical::Motion* motions = physicalComponent->GetMotions();
			int numMotions = physicalComponent->GetNumMotions();
			if(numMotions != 0)
			{
				m_springIndex = Min(m_springIndex, numMotions-1);
				crMotionDescription& descr = motions[m_springIndex].m_Descr;
				m_pSpringGravity = m_pLinearPhysicalExpressionGroup->AddVector("Gravity", (Vec3V*)&descr.m_Gravity, -100.f, 100.f, 0.05f);
				m_pSpringDirection = m_pLinearPhysicalExpressionGroup->AddVector("Direction", (Vec3V*)&descr.m_Direction, -100.f, 100.f, 0.05f);
				PopulateDofWidgets(*m_pLinearPhysicalExpressionGroup, m_linearSpringDofs, "Linear", descr.m_Linear);
				PopulateDofWidgets(*m_pLinearPhysicalExpressionGroup, m_angularSpringDofs, "Angular", descr.m_Angular);
			}
		}
	}
}

void CAnimViewer::SelectSpring()
{
	if (m_pLinearPhysicalExpressionGroup)
	{
		if (m_pSpringGravity)
		{
			m_pLinearPhysicalExpressionGroup->Remove(*m_pSpringGravity);
			m_pSpringGravity=NULL;
		}
		if (m_pSpringDirection)
		{
			m_pLinearPhysicalExpressionGroup->Remove(*m_pSpringDirection);
			m_pSpringDirection=NULL;
		}

		ClearDofWidgets(*m_pLinearPhysicalExpressionGroup, m_linearSpringDofs);
		ClearDofWidgets(*m_pLinearPhysicalExpressionGroup, m_angularSpringDofs);

		PopulateSpringWidgets();
	}
}

#if ENABLE_BLENDSHAPES
void CAnimViewer::SelectBlendShape()
{
	CPed* pPed = static_cast<CPed*>(m_pDynamicEntity.Get());
	if (!pPed)
	{
		return;
	}

	// Keep the bone index in range
	int numberOfBlendShapes = 0;

	rmcDrawable* pHeadComponent = pPed->GetComponent(PV_COMP_HEAD);
	if (pHeadComponent)
	{
		grbTargetManager* pBlendShapeManager = m_pDynamicEntity->GetTargetManager();
		if(pBlendShapeManager)
		{
			atMap<u16, datOwner<grbTarget> >::ConstIterator iter = pBlendShapeManager->GetBlendTargetIterator();
			while(iter)
			{
				const grbTarget *pTarget = iter.GetData();
				if (pTarget)
				{
					++numberOfBlendShapes;
				}
				++iter;
			}
		}
	}

	if (m_blendShapeIndex>=numberOfBlendShapes)
	{
		m_blendShapeIndex = 0;
	}
	if (m_blendShapeIndex<0)
	{
		m_blendShapeIndex = numberOfBlendShapes;
	}
}
#endif // ENABLE_BLENDSHAPES

/*
void CAnimViewer::UpdateBoneIndex()
{
if(!m_pDynamicEntity)
{
m_pDynamicEntity = FindPlayerPed();
}

// Keep the bone index in range
int numberOfBones = m_pDynamicEntity->GetSkeletonData().GetNumBones();
numberOfBones--;
if (m_boneIndex>numberOfBones)
{
m_boneIndex = 0;
}
if (m_boneIndex<0)
{
m_boneIndex = numberOfBones;
}

// Get the rotation min and max for the given bone index
const crBoneData* boneData = m_pDynamicEntity->GetSkeletonData().GetBoneData(m_boneIndex);
m_rotationMin = boneData->GetRotationMin();
m_rotationMax = boneData->GetRotationMax();

// Get the rotation for the given bone index
const crBone &bone = m_pDynamicEntity->GetSkeleton()->GetBone(m_boneIndex);
const Matrix34 &mat = bone.GetLocalMtx();
mat.ToEulersXYZ(m_rotation);
}
*/

//void CAnimViewer::UpdatePriority()
//{
//	// Keep the bone index in range
//	int numberOfPriorities = EANIMPRIORITY_MAX;
//	numberOfPriorities--;
//	if (m_priority>numberOfPriorities)
//	{
//		m_priority = 0;
//	}
//	if (m_priority<0)
//	{
//		m_priority = numberOfPriorities;
//	}
//
//	// Is the selected animation player valid?
//	if (!m_pAnimPlayer) return;
//}

void CAnimViewer::SelectMotionClipSet()
{
	if (m_pDynamicEntity && m_pDynamicEntity->GetIsTypePed())
	{
		CPed* pPed = static_cast<CPed*>(m_pDynamicEntity.Get());
		CTaskMotionBase* pTask = pPed->GetPrimaryMotionTask();

		if (pTask)
		{
			fwMvClipSetId selectedSet(fwClipSetManager::GetClipSetNames()[m_motionClipSetIndex]);
			fwClipSetManager::StreamIn_DEPRECATED(selectedSet);
			pTask->SetOnFootClipSet(selectedSet);
		}
	}
}

void CAnimViewer::SelectWeaponClipSet()
{
	if (m_pDynamicEntity && m_pDynamicEntity->GetIsTypePed())
	{
		CPed* pPed = static_cast<CPed*>(m_pDynamicEntity.Get());
		CTaskMotionBase* pTask = pPed->GetPrimaryMotionTask();

		if (pTask)
		{
			fwMvClipSetId selectedSet(fwClipSetManager::GetClipSetNames()[m_weaponClipSetIndex]);
			fwClipSetManager::StreamIn_DEPRECATED(selectedSet);
			pTask->SetOverrideWeaponClipSet(selectedSet);
		}
	}
}

void CAnimViewer::ClearWeaponClipSet()
{
	if (m_pDynamicEntity && m_pDynamicEntity->GetIsTypePed())
	{
		CPed* pPed = static_cast<CPed*>(m_pDynamicEntity.Get());
		CTaskMotionBase* pTask = pPed->GetPrimaryMotionTask();

		if (pTask)
		{
			pTask->ClearOverrideWeaponClipSet();
		}
	}
}

void CAnimViewer::SelectFacialClipSet()
{
	if (m_pDynamicEntity && m_pDynamicEntity->GetIsTypePed())
	{
		CPed *pPed = static_cast< CPed * >(m_pDynamicEntity.Get());
		CFacialDataComponent *pFacialDataComponent = pPed->GetFacialData();
		if (pFacialDataComponent)
		{
			fwMvClipSetId selectedSet(fwClipSetManager::GetClipSetNames()[m_facialClipSetIndex]);
			if(m_facialClipSetHelper.Request(selectedSet, true))
			{
				pFacialDataComponent->SetFacialClipSet(pPed, selectedSet);
			}
		}
	}
}

void CAnimViewer::ToggleEnableFacial()
{
	if (m_pDynamicEntity && m_pDynamicEntity->GetIsTypePed())
	{
		CPed *pPed = static_cast< CPed * >(m_pDynamicEntity.Get());
		fwAnimDirectorComponentFacialRig *pAnimDirectorComponentFacialRig = static_cast< fwAnimDirectorComponentFacialRig * >(pPed->GetAnimDirector()->GetComponentByType(fwAnimDirectorComponent::kComponentTypeFacialRig));
		if (pAnimDirectorComponentFacialRig)
		{
			pAnimDirectorComponentFacialRig->SetEnable(m_bEnableFacial);
		}
	}
}



void CAnimViewer::SelectPed()
{
	CPed* pOldPlayer = static_cast<CPed*>(m_pDynamicEntity.Get());

	if (!pOldPlayer)
	{
		pOldPlayer = FindPlayerPed();
	}

	animAssert(pOldPlayer);

	if(m_pDynamicEntity)
	{
		m_pDynamicEntity = NULL;
	}

	// remove old model if appropriate (i.e. ref count is 0 and don't delete flags not set)
	if (pOldPlayer->GetBaseModelInfo()->GetNumRefs() == 0)
	{
		if  (!(CModelInfo::GetAssetStreamFlags(pOldPlayer->GetModelId()) & STR_DONTDELETE_MASK))
		{
			CModelInfo::RemoveAssets(pOldPlayer->GetModelId());
		}
	}

	// Load the selected model
	fwModelId modelId = CModelInfo::GetModelIdFromName(m_modelNames[m_modelIndex]);
	if (!CModelInfo::HaveAssetsLoaded(modelId))
	{
		CModelInfo::RequestAssets(modelId, STRFLAG_DONTDELETE|STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD);
		CStreaming::LoadAllRequestedObjects(true);
	}

	// destroy current player ped and recreate with new model
	m_pDynamicEntity = CGameWorld::ChangePedModel(*pOldPlayer, modelId);
	animAssert(m_pDynamicEntity);

	// let the ped variation widgets know about the new ped

	CPed* pPed = static_cast<CPed*>(m_pDynamicEntity.Get());
	CPedVariationDebug::SetDebugPed(pPed, m_modelIndex);
	for (int i=0;  i<PV_MAX_COMP; i++)
	{
		pPed->SetVariation(static_cast<ePedVarComp>(i), 0, 0, 0, 0, 0, false);
    }

	CPedPropsMgr::WidgetUpdate();

	// The skeleton may have changed re-init the bone & spring names
	InitBoneNames();
	InitSpringNames();

	// The skeleton may have changed re-init the blendshape names
	//InitBlendshapeNames();

	// Set the task of the new player
	ToggleUninterruptableTask();
}

void CAnimViewer::SpawnSelectedObject()
{
	// delete the existing object if necessary
	if(m_spawnedObject)
	{
		CGameWorld::Remove(m_spawnedObject);

		if(m_spawnedObject->GetIsTypeObject())
			CObjectPopulation::DestroyObject((CObject*)m_spawnedObject.Get());
		else
			delete m_spawnedObject;
	}

	m_spawnedObject = NULL;

	//get the model info
	fwModelId nModelId;
	CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfoFromName(m_objectSelector.GetSelectedObjectName(), &nModelId);
	if(pModelInfo==NULL)
		return;

	//work out where to put the object
	Matrix34 testMat;
	testMat.Identity();

	Vector3 vecCreateOffset = camInterface::GetFront();
	vecCreateOffset.z = 0.0f;
	vecCreateOffset *= 2.0f + pModelInfo->GetBoundingSphereRadius();

	camDebugDirector& debugDirector = camInterface::GetDebugDirector();
	testMat.d = debugDirector.IsFreeCamActive() ? camInterface::GetPos() : CGameWorld::FindLocalPlayerCoors();
	testMat.d += vecCreateOffset;

	if(debugDirector.IsFreeCamActive())
		testMat.Set3x3(camInterface::GetMat());
	else if(CGameWorld::FindLocalPlayer())
		testMat.Set3x3(MAT34V_TO_MATRIX34(CGameWorld::FindLocalPlayer()->GetMatrix()));

	//spawn the new object
	if( pModelInfo )
	{
		if(pModelInfo->GetIsTypeObject())
		{
			m_spawnedObject = CObjectPopulation::CreateObject(nModelId, ENTITY_OWNEDBY_RANDOM, true);
		}
	}

	//add the object to the world
	if(m_spawnedObject)
	{
		m_spawnedObject->SetMatrix(testMat);
		CGameWorld::Add(m_spawnedObject, CGameWorld::OUTSIDE );
	}

	//If the entity being is an env cloth then we need to transform the cloth
	//verts to the entity frame.
	if(m_spawnedObject && m_spawnedObject->GetCurrentPhysicsInst() && m_spawnedObject->GetCurrentPhysicsInst()->GetArchetype())
	{
		if(m_spawnedObject->GetCurrentPhysicsInst()->GetArchetype()->GetTypeFlags()==ArchetypeFlags::GTA_ENVCLOTH_OBJECT_TYPE)
		{
			//Now transform the cloth verts to the frame of the cloth entity.
			phInst* pEntityInst=m_spawnedObject->GetCurrentPhysicsInst();
			Assertf(dynamic_cast<fragInst*>(pEntityInst), "Physical with cloth must be a frag");
			fragInst* pEntityFragInst=static_cast<fragInst*>(pEntityInst);
			Assertf(pEntityFragInst->GetCached(), "Cloth entity has no cache entry");
			if(pEntityFragInst->GetCached())
			{
				fragCacheEntry* pEntityCacheEntry=pEntityFragInst->GetCacheEntry();
				Assertf(pEntityCacheEntry, "Cloth entity has a null cache entry");
				fragHierarchyInst* pEntityHierInst=pEntityCacheEntry->GetHierInst();
				Assertf(pEntityHierInst, "Cloth entity has a null hier inst");
				environmentCloth* pEnvCloth=pEntityHierInst->envCloth;
				Matrix34 clothMatrix=MAT34V_TO_MATRIX34(m_spawnedObject->GetMatrix());

				pEnvCloth->SetInitialPosition( VECTOR3_TO_VEC3V(clothMatrix.d) );
			}
		}
	}//	if(gpDisplayObject && gpDisplayObject->GetCurrentPhysicsInst() && gpDisplayObject->GetCurrentPhysicsInst()->GetArchetype())...
}


void CAnimViewer::SelectLastSpawnedObject()
{
	if(m_spawnedObject)
	{
		FocusEntityChanged(m_spawnedObject);
	}
}

void CAnimViewer::ClearPrimaryTaskOnSelectedPed()
{
	if (m_pDynamicEntity->GetIsTypePed())
	{
		CPed* pPed = static_cast<CPed*>(m_pDynamicEntity.Get());
		CPedIntelligence *pPedIntelligence = pPed->GetPedIntelligence();
		pPedIntelligence->ClearTasks(true, false);
	}

	if (m_pDynamicEntity->GetIsTypeVehicle())
	{
		CVehicle *pVehicle = static_cast<CVehicle*>(m_pDynamicEntity.Get());
		CVehicleIntelligence *pVehicleIntelligence = pVehicle->GetIntelligence();
		pVehicleIntelligence->GetTaskManager()->ClearTask(VEHICLE_TASK_TREE_PRIMARY, VEHICLE_TASK_PRIORITY_PRIMARY);
	}

	if (m_pDynamicEntity->GetIsTypeObject())
	{
		CObject *pObject = static_cast<CObject*>(m_pDynamicEntity.Get()); 
		CObjectIntelligence *pObjectIntelligence = pObject->GetObjectIntelligence();
		pObjectIntelligence->GetTaskManager()->ClearTask(CObjectIntelligence::OBJECT_TASK_TREE_PRIMARY, CObjectIntelligence::OBJECT_TASK_PRIORITY_PRIMARY);
	}
}

void CAnimViewer::ClearSecondaryTaskOnSelectedPed()
{
	if (m_pDynamicEntity->GetIsTypePed())
	{
		CPed* pPed = static_cast<CPed*>(m_pDynamicEntity.Get());
		CPedIntelligence *pPedIntelligence = pPed->GetPedIntelligence();
		pPedIntelligence->ClearTasks(false, true);
	}

	if (m_pDynamicEntity->GetIsTypeVehicle())
	{
		CVehicle *pVehicle = static_cast<CVehicle*>(m_pDynamicEntity.Get());
		CVehicleIntelligence *pVehicleIntelligence = pVehicle->GetIntelligence();
		pVehicleIntelligence->GetTaskManager()->ClearTask(VEHICLE_TASK_TREE_SECONDARY, VEHICLE_TASK_SECONDARY_ANIM);
	}

	if (m_pDynamicEntity->GetIsTypeObject())
	{
		CObject *pObject = static_cast<CObject*>(m_pDynamicEntity.Get()); 
		CObjectIntelligence *pObjectIntelligence = pObject->GetObjectIntelligence();
		pObjectIntelligence->GetTaskManager()->ClearTask(CObjectIntelligence::OBJECT_TASK_TREE_SECONDARY, CObjectIntelligence::OBJECT_TASK_SECONDARY_ANIM);
	}
}

void CAnimViewer::ClearMotionTaskOnSelectedPed()
{
	if (m_pDynamicEntity->GetIsTypePed())
	{
		CPed* pPed = static_cast<CPed*>(m_pDynamicEntity.Get());
		pPed->GetMovePed().SetMotionTaskNetwork(NULL);
	}
}

//////////////////////////////////////////////////////////////////////////
// Adds an entry for the selected bone in the bone history array
// setting the flag m_autoAddBoneHistoryEveryFrame will create one of
// these each frame
//////////////////////////////////////////////////////////////////////////

void CAnimViewer::AddBoneHistoryEntry()
{
	if (m_pDynamicEntity )
	{
		sBoneHistory* newEntry = NULL;

		//lazily grow the bone history structure
		if (m_boneHistoryArray.GetCount()<NUM_DEBUG_MATRICES)
		{
			newEntry = &m_boneHistoryArray.Grow();
		}
		else
		{
			newEntry = &m_boneHistoryArray[m_matIndex];
		}

		Matrix34 globalMat;
		m_pDynamicEntity->GetGlobalMtx(m_boneIndex, globalMat);

		newEntry->m_boneMatrix	=  globalMat;
		newEntry->m_moverMatrix =  MAT34V_TO_MATRIX34(m_pDynamicEntity->GetMatrix());

		//increment the counter
		m_matIndex++;
		if (m_matIndex < NUM_DEBUG_MATRICES)
		{
		}
		else
		{
			m_matIndex = 0;
		}
	}
}

void CAnimViewer::ClearBoneHistory()
{
	m_matIndex = 0;

	m_boneHistoryArray.clear();
}


void CAnimViewer::AddDebugSkeleton(Color32 color, const char * debugText, bool bWhenPaused)
{
	AddDebugSkeleton(*m_pDynamicEntity, color, debugText, bWhenPaused);
}

void CAnimViewer::AddDebugSkeleton(CDynamicEntity& dynamicEntity, Color32 color, bool bWhenPaused)
{
	AddDebugSkeleton(dynamicEntity,color,"AnimViewer", bWhenPaused);	
}

void CAnimViewer::AddDebugSkeleton(CDynamicEntity &dynamicEntity, Color32 color, const char * debugText, bool bWhenPaused)
{
	USE_MEMBUCKET(MEMBUCKET_ANIMATION);
	if ((!fwTimer::IsUserPaused() || fwTimer::GetSingleStepThisFrame()) || bWhenPaused)
	{

		static int reservedCharacters = 16;
		atString description(debugText);

		if (description.length()>MAX_DEBUG_SKELETON_DESCRIPTION - unsigned(reservedCharacters))
		{
			description.Truncate(MAX_DEBUG_SKELETON_DESCRIPTION - reservedCharacters);
		}
		
		sDebugSkeleton* newSkeleton = NULL;

		if (m_debugSkeletons.GetCount() < MAX_DEBUG_SKELETON)
		{
			newSkeleton = &m_debugSkeletons.Grow();
		}
		else
		{
			newSkeleton = &m_debugSkeletons[m_skeletonIndex];
		}

		sprintf( &newSkeleton->m_description[0] ,"SI:%d F:%d -%s", m_skeletonIndex, fwTimer::GetFrameCount(), description.c_str());

		newSkeleton->m_color = color;
		newSkeleton->m_parentMat = MAT34V_TO_MATRIX34(dynamicEntity.GetMatrix());
		
		//need to copy the whole skeleton now since the player no longer has the same skeleton as the other peds
		newSkeleton->m_skelParentMat = RCC_MATRIX34(*dynamicEntity.GetSkeleton()->GetParentMtx()); //take a copy of the skeleton parent matrix
		newSkeleton->m_skeleton.Init(dynamicEntity.GetSkeleton()->GetSkeletonData());
		newSkeleton->m_skeleton.CopyFrom(*(dynamicEntity.GetSkeleton()));
		newSkeleton->m_skeleton.SetParentMtx(&RCC_MAT34V(newSkeleton->m_skelParentMat));

		m_skeletonIndex++;
		if (!(m_skeletonIndex<MAX_DEBUG_SKELETON))
		{
			m_skeletonIndex	= 0;
		}
	}
}

void CAnimViewer::AddDebugSkeleton(const crSkeleton &skel, Color32 color, const char * debugText, bool bWhenPaused)
{
	USE_MEMBUCKET(MEMBUCKET_ANIMATION);
	if ((!fwTimer::IsUserPaused() || fwTimer::GetSingleStepThisFrame()) || bWhenPaused)
	{

		static int reservedCharacters = 16;
		atString description(debugText);

		if (description.length()>MAX_DEBUG_SKELETON_DESCRIPTION - unsigned(reservedCharacters))
		{
			description.Truncate(MAX_DEBUG_SKELETON_DESCRIPTION - reservedCharacters);
		}

		sDebugSkeleton* newSkeleton = NULL;

		if (m_debugSkeletons.GetCount() < MAX_DEBUG_SKELETON)
		{

			newSkeleton = &m_debugSkeletons.Grow();
		}
		else
		{
			newSkeleton = &m_debugSkeletons[m_skeletonIndex];
		}

		sprintf( &newSkeleton->m_description[0] ,"SI:%d F:%d -%s", m_skeletonIndex, fwTimer::GetFrameCount(), description.c_str());

		newSkeleton->m_color = color;
		newSkeleton->m_parentMat = RCC_MATRIX34(*skel.GetParentMtx());

		//need to copy the whole skeleton now since the player no longer has the same skeleton as the other peds
		newSkeleton->m_skelParentMat = RCC_MATRIX34(*skel.GetParentMtx()); //take a copy of the skeleton parent matrix
		newSkeleton->m_skeleton = skel;
		newSkeleton->m_skeleton.SetParentMtx(&RCC_MAT34V(newSkeleton->m_skelParentMat));

		m_skeletonIndex++;
		if (!(m_skeletonIndex<MAX_DEBUG_SKELETON))
		{
			m_skeletonIndex	= 0;
		}
	}
}

void CAnimViewer::AddDebugSkeleton(CDynamicEntity& UNUSED_PARAM(dynamicEntity), const crClip& UNUSED_PARAM(anim), Matrix34& UNUSED_PARAM(fixupTransform), float UNUSED_PARAM(time), Color32 UNUSED_PARAM(color))
{
	//m_skeletonColorArray[m_skeletonIndex] = color;
	//m_skeletonParentMatArray[m_skeletonIndex] =  fixupTransform;
	////m_skeletonParentMatArray[m_skeletonIndex].Dot(dynamicEntity.GetMatrix());

	//const crSkeletonData &skelData = dynamicEntity.GetSkeletonData();

	//crFrame animFrame;
	//animFrame.InitCreateBoneAndMoverDofs(skelData);
	//animFrame.IdentityFromSkel(skelData);
	//anim.Composite(animFrame, time, 0.0f);

	//// Apply the mover to the root

	//// Does the translation track for the root bone dof exist ?
	//Vector3 displacement(VEC3_ZERO);
	//if (animFrame.GetVector3(kTrackMoverTranslation, 0, displacement))
	//{
	//	// Apply the mover track translation that need to be reapplied to the root bone.
	//	Vector3 currentRootTranslation(VEC3_ZERO);
	//	animFrame.GetBoneTranslation(0, currentRootTranslation);
	//	Vector3 newRootTranslation = currentRootTranslation + displacement;
	//	animFrame.SetBoneTranslation(0, newRootTranslation);
	//}

	//// Does the rotation track for the root bone dof exist ?
	//Quaternion rotation(Quaternion::sm_I);
	//if (animFrame.GetQuaternion(kTrackMoverRotation, 0, rotation))
	//{
	//	// Apply the mover track rotation that need to be reapplied to the root bone.
	//	Quaternion currentRootRotation(Quaternion::sm_I);
	//	animFrame.GetBoneRotation(0, currentRootRotation);

	//	Quaternion newRootRotation(Quaternion::sm_I);
	//	newRootRotation.Multiply(rotation, currentRootRotation);
	//	animFrame.SetBoneRotation(0, newRootRotation);
	//}

	//animFrame.Pose(m_skeletonArray[m_skeletonIndex], false, NULL);

	//m_skeletonArray[m_skeletonIndex].SetParentMtx(&fixupTransform);
	//m_skeletonArray[m_skeletonIndex].Update();
	//m_skeletonArray[m_skeletonIndex].SetParentMtx(&M34_IDENTITY);

	//m_skeletonIndex++;
	//if (!(m_skeletonIndex<MAX_DEBUG_SKELETON))
	//{
	//	m_skeletonIndex	= 0;
	//}
}

#if __DEV


void CAnimViewer::DumpPosematcher()
{
	if (m_pDynamicEntity)
	{
	//m_pDynamicEntity->GetAnimDirector()->GetComponent<fwAnimDirectorComponentRagDoll>()->GetPoseMatcher().Dump();
	}
}

void CAnimViewer::DumpNMBS()
{
	eNmBlendOutSet blendOutSets[] = 
	{
		NMBS_INVALID,

		NMBS_STANDING,
		NMBS_STANDING_AIMING_CENTRE,
		NMBS_STANDING_AIMING_LEFT,
		NMBS_STANDING_AIMING_RIGHT,

		NMBS_STANDING_AIMING_CENTRE_PISTOL_HURT,
		NMBS_STANDING_AIMING_LEFT_PISTOL_HURT,
		NMBS_STANDING_AIMING_RIGHT_PISTOL_HURT,

		NMBS_STANDING_AIMING_CENTRE_RIFLE_HURT,
		NMBS_STANDING_AIMING_LEFT_RIFLE_HURT,
		NMBS_STANDING_AIMING_RIGHT_RIFLE_HURT,

		NMBS_STANDING_AIMING_CENTRE_RIFLE,
		NMBS_STANDING_AIMING_LEFT_RIFLE,
		NMBS_STANDING_AIMING_RIGHT_RIFLE,

		NMBS_INJURED_STANDING,

		NMBS_STANDARD_GETUPS,
		NMBS_PLAYER_GETUPS,
		NMBS_PLAYER_MP_GETUPS,
		NMBS_SLOW_GETUPS,
		NMBS_FAST_GETUPS,
		NMBS_INJURED_GETUPS,
		NMBS_INJURED_ARMED_GETUPS,

		NMBS_ARMED_1HANDED_GETUPS,
		NMBS_ARMED_1HANDED_SHOOT_FROM_GROUND,
		NMBS_ARMED_2HANDED_GETUPS,
		NMBS_ARMED_2HANDED_SHOOT_FROM_GROUND,

		NMBS_CUFFED_GETUPS,
		NMBS_WRITHE,
		NMBS_DRUNK_GETUPS,

		NMBS_PANIC_FLEE,
		NMBS_PANIC_FLEE_INJURED,
		NMBS_ARREST_GETUPS,

		NMBS_FULL_ARREST_GETUPS,

		NMBS_SWIMMING_GETUPS,
		NMBS_DIVING_GETUPS,
		NMBS_SKYDIVE_GETUPS,
	};

	Displayf("NMBS, ClipSet, ClipName, ClipPath, FbxPath, PointCloud");
	for(int i=0; i<NELEM(blendOutSets); ++i)
	{
		CNmBlendOutSet* nmbSet = CNmBlendOutSetManager::GetBlendOutSet(blendOutSets[i]);
		if(nmbSet)
		{
			for(int j=0; j<nmbSet->m_items.GetCount(); ++j)
			{
				char clipSetBuf[256]; clipSetBuf[0] = '\0';
				char clipNameBuf[256]; clipNameBuf[0] = '\0';
				char clipPathBuf[256]; clipPathBuf[0] = '\0';
				char clipFbxBuf[256]; clipFbxBuf[0] = '\0';

				CNmBlendOutItem* nmbItem = nmbSet->m_items[j];
				if(nmbItem->HasClipSet())
				{
					fwClipSet *clipSet = fwClipSetManager::GetClipSet(nmbItem->GetClipSet());
					if(clipSet)
					{
						fwClipDictionaryLoader loader(clipSet->GetClipDictionaryIndex().Get());
						if(loader.IsLoaded())
						{
							safecpy(clipSetBuf, clipSet->GetClipDictionaryName().TryGetCStr());
							if(nmbItem->GetClip() != CLIP_ID_INVALID)
							{
								crClip *clip = clipSet->GetClip(nmbItem->GetClip());
								if(clip)
								{
									safecpy(clipNameBuf, clip->GetName()+6);

									const char *localPath = CDebugClipDictionary::FindLocalDirectoryForDictionary(clipSet->GetClipDictionaryName().TryGetCStr());
									if(strlen(localPath)>0)
									{
										char clipFilePath[512];
										sprintf(clipFilePath, "%s\\%s", localPath, clip->GetName()+6);

										crClip* loadedClip = crClip::AllocateAndLoad(clipFilePath, NULL);
										if(loadedClip)
										{
											safecpy(clipPathBuf, loadedClip->GetName());
											if(loadedClip->GetProperties())
											{
												crProperty* propFbx = loadedClip->GetProperties()->FindProperty("FBXFile_DO_NOT_RESOURCE");
												if(propFbx)
												{
													crPropertyAttributeStringAccessor accessorFbx(propFbx->GetAttribute("FBXFile_DO_NOT_RESOURCE"));
													if(accessorFbx.Valid())
													{
														safecpy(clipFbxBuf, accessorFbx.GetPtr()->c_str());
													}
												}
											}

											delete loadedClip;
										}
									}
								}
							}
						}
					}
				}

				Displayf("%s, %s, %s, %s, %s, %d",CNmBlendOutSetManager::GetBlendOutSetName(blendOutSets[i]), clipSetBuf, clipNameBuf, clipPathBuf, clipFbxBuf, nmbItem->ShouldAddToPointCloud());
			}
		}
	}
}

#endif //__DEV


void CAnimViewer::AddDebugSkeleton()
{
	AddDebugSkeleton(*m_pDynamicEntity, Color_green, true);
}

void CAnimViewer::RenderSkeleton(const crSkeleton& skeleton, Color32 color, crFrameFilter* filter)
{
	const crSkeletonData &skeletonData = skeleton.GetSkeletonData();
	int totalBones = skeletonData.GetNumBones();
	for (int boneIndex = 0; boneIndex < totalBones; boneIndex++)
	{
		const crBoneData *boneData = skeletonData.GetBoneData(boneIndex);

		float inOutWeight;
		if (filter != NULL && !filter->FilterDof(kTrackBoneTranslation, boneData->GetBoneId(), inOutWeight) && !filter->FilterDof(kTrackBoneRotation, boneData->GetBoneId(), inOutWeight))
		{
			continue;
		}

		Mat34V mBone;
		skeleton.GetGlobalMtx(boneIndex, mBone);
		Vec3V a(mBone.d());

		for (const crBoneData *child = boneData->GetChild(); child; child = child->GetNext())
		{
			float inOutWeight;
			if (filter != NULL && !filter->FilterDof(kTrackBoneTranslation, child->GetBoneId(), inOutWeight) && !filter->FilterDof(kTrackBoneRotation, child->GetBoneId(), inOutWeight))
			{
				continue;
			}

			skeleton.GetGlobalMtx(child->GetIndex(), mBone);
			grcDebugDraw::Line(a,mBone.d(),color);
		}
	}
}

void CAnimViewer::RenderSkeletonUsingLocals(const crSkeleton& skeleton, Color32 color)
{
	// GS - Quick hack to render using the local matrices
	crSkeleton tempSkel;
	tempSkel.Init(skeleton.GetSkeletonData(), skeleton.GetParentMtx());
	tempSkel.CopyFrom(skeleton);

	tempSkel.Update();

	RenderSkeleton(tempSkel, color);
}

//////////////////////////////////////////////////////////////////////////
// Old CAnimDebug implementation
//////////////////////////////////////////////////////////////////////////



void CAnimViewer::ScrollUp()
{
	m_startLine--;

	if (m_startLine<0)
	{
		m_startLine = 0;
	}
	else
	{
		m_endLine--;
		if (m_endLine<MAX_LINES_IN_TABLE)
		{
			m_endLine = MAX_LINES_IN_TABLE;
		}
	}
}

void CAnimViewer::ScrollDown()
{
	m_endLine++;

	if (m_endLine>m_selectedCount)
	{
		m_endLine = m_selectedCount;
	}
	else
	{
		m_startLine++;
	}
}

void CAnimViewer::ToggleSortByName()
{
	m_sortBySize = !m_sortByName;
}

void CAnimViewer::ToggleSortBySize()
{
	m_sortByName = !m_sortBySize;
}

struct AnimCounts {
	int pedCount;
	int pedOnscreenCount;
	int pedOffscreenCount;

	int combinedCount;
	int combinedOnscreenCount;
	int combinedOffscreenCount;

	int lowLODCount;
	int highLODCount;
};

void CAnimViewer::SceneUpdateCountAnimsCb(fwEntity &entity, void *userData)
{
	AnimCounts *counts = (AnimCounts *) userData;
	CDynamicEntity* pDynamicEntity = (CDynamicEntity*)&entity;
	Assert(pDynamicEntity->GetIsDynamic());

	if(pDynamicEntity->IsBaseFlagSet(fwEntity::REMOVE_FROM_WORLD))
		return;

	if(pDynamicEntity->GetIsTypePed())
	{		
		counts->pedCount++;
		counts->combinedCount++;
		if (pDynamicEntity->GetIsVisibleInSomeViewportThisFrame())
		{
			counts->pedOnscreenCount++;
			counts->combinedOnscreenCount++;
		}
		else
		{
			counts->pedOffscreenCount++;
			counts->combinedOffscreenCount++;
		}

		// Get a pointer to the modelInfo
		CPedModelInfo* pModelInfo = static_cast< CPedModelInfo* >(pDynamicEntity->GetBaseModelInfo());
		if (pModelInfo)
		{
			if (pDynamicEntity->GetIsTypePed())
			{	
				if (static_cast<CPed*>(pDynamicEntity)->GetPedDrawHandler().GetVarData().GetIsHighLOD())
				{
					counts->highLODCount++;
				}
				else
				{
					counts->lowLODCount++;
				}
			}
		}
	}
}

int GetMaxLength(atArray< atString > &strings)
{
	int maxLength = 0;
	for(int i = 0; i < strings.GetCount(); i ++)
	{
		int length = strings[i].GetLength();
		if(length > maxLength)
		{
			maxLength = length;
		}
	}
	return maxLength;
}

void FindClipDictionaryMetadataFilesCallback(const fiFindData &data, void *user)
{
	atArray< atString > &files = *reinterpret_cast< atArray< atString > * >(user);

	atString file(ASSET.FileName(data.m_Name));
	file.Lowercase();

	if(file.EndsWith(".xml"))
	{
		files.PushAndGrow(file);
	}
}

void CAnimViewer::OldDebugRender()
{
	int iNoOfTexts			= 0;
	int vertDist			= 1;
	int& horizontalBorder	= m_horizontalScroll;
	int& verticalBorder		= m_verticalScroll;
	int horizontalIncrement = 10;
	Color32 textColor(Color_white);
	char debugText[100];

	// Reset all the selected memory totals
	int memoryGroupCount = fwClipSetManager::GetMemoryGroupMetadataCount();
	for (int i=0; i<memoryGroupCount; i++)
	{			
		fwMemoryGroupMetadata* pMemoryGroupMetadata = fwClipSetManager::GetMemoryGroupMetadataByIndex(i);
		if (pMemoryGroupMetadata)
		{
			pMemoryGroupMetadata->SetMemorySelected(0);
		}
	}

	if (m_showAnimLODInfo)
	{
		// loop over the peds count how many are in an active viewport
		AnimCounts counts;
		sysMemSet(&counts, 0, sizeof(AnimCounts));

		// Check that the entity is not in the process control list twice
		fwSceneUpdate::RunCustomCallbackBits(CGameWorld::SU_START_ANIM_UPDATE_PRE_PHYSICS, SceneUpdateCountAnimsCb, &counts);
		fwSceneUpdate::RunCustomCallbackBits(CGameWorld::SU_START_ANIM_UPDATE_PRE_PHYSICS_PASS2, SceneUpdateCountAnimsCb, &counts);

		formatf(debugText, "Total number of peds %d\n", counts.pedCount);
		grcDebugDraw::PrintToScreenCoors(debugText, horizontalBorder + 0,   verticalBorder + iNoOfTexts*vertDist);
		iNoOfTexts++;

		formatf(debugText, "Number of onscreen peds %d\n", counts.pedOnscreenCount);
		grcDebugDraw::PrintToScreenCoors(debugText, horizontalBorder + 0,   verticalBorder + iNoOfTexts*vertDist);
		iNoOfTexts++;

		formatf(debugText, "Number of offscreen peds %d\n", counts.pedOffscreenCount);
		grcDebugDraw::PrintToScreenCoors(debugText, horizontalBorder + 0,   verticalBorder + iNoOfTexts*vertDist);
		iNoOfTexts++;

		formatf(debugText, "Total number of peds %d\n", counts.pedCount);
		grcDebugDraw::PrintToScreenCoors(debugText, horizontalBorder + 0,   verticalBorder + iNoOfTexts*vertDist);
		iNoOfTexts++;

		formatf(debugText, "Number of onscreen combined %d\n", counts.combinedOnscreenCount);
		grcDebugDraw::PrintToScreenCoors(debugText, horizontalBorder + 0,   verticalBorder + iNoOfTexts*vertDist);
		iNoOfTexts++;

		formatf(debugText, "Number of offscreen combined %d\n", counts.combinedOffscreenCount);
		grcDebugDraw::PrintToScreenCoors(debugText, horizontalBorder + 0,   verticalBorder + iNoOfTexts*vertDist);
		iNoOfTexts++;

		formatf(debugText, "Total number of combined %d\n", counts.combinedCount);
		grcDebugDraw::PrintToScreenCoors(debugText, horizontalBorder + 0,   verticalBorder + iNoOfTexts*vertDist);
		iNoOfTexts++;

		formatf(debugText, "Number of low LOD combined %d\n", counts.lowLODCount);
		grcDebugDraw::PrintToScreenCoors(debugText, horizontalBorder + 0,   verticalBorder + iNoOfTexts*vertDist);
		iNoOfTexts++;

		formatf(debugText, "Number of high LOD combined %d\n", counts.highLODCount);
		grcDebugDraw::PrintToScreenCoors(debugText, horizontalBorder + 0,   verticalBorder + iNoOfTexts*vertDist);
		iNoOfTexts++;

		iNoOfTexts++;

#if ANIMDEBUG_STATS
		//PF_INCREMENTBY(CAnimViewer_PedsOnscreen, pedOnscreenCount);
		//PF_INCREMENTBY(CAnimViewer_PedsOffscreen, pedOffscreenCount);
		//PF_INCREMENTBY(CAnimViewer_combinedOnScreen, combinedOnscreenCount);
		//PF_INCREMENTBY(CAnimViewer_combinedOffscreen, combinedOffscreenCount);
#endif // ANIMDEBUG_STATS

	}

	if (m_showAnimPoolInfo)
	{
		sprintf(debugText, "Number of fwAnimDirector %d/%d\n", fwAnimDirectorPooledObject::GetPool()->GetNoOfFreeSpaces(), fwAnimDirectorPooledObject::GetPool()->GetSize());
		grcDebugDraw::PrintToScreenCoors(debugText, horizontalBorder + 0,   verticalBorder + iNoOfTexts*vertDist);
		iNoOfTexts++;

		sprintf(debugText, "Number of CMovePed %d/%d\n", CMovePedPooledObject::GetPool()->GetNoOfFreeSpaces(), CMovePedPooledObject::GetPool()->GetSize());
		grcDebugDraw::PrintToScreenCoors(debugText, horizontalBorder + 0,   verticalBorder + iNoOfTexts*vertDist);
		iNoOfTexts++;

		sprintf(debugText, "Number of CMoveAnimatedBuilding %d/%d\n", CMoveAnimatedBuildingPooledObject::GetPool()->GetNoOfFreeSpaces(), CMoveAnimatedBuildingPooledObject::GetPool()->GetSize());
		grcDebugDraw::PrintToScreenCoors(debugText, horizontalBorder + 0,   verticalBorder + iNoOfTexts*vertDist);
		iNoOfTexts++;

		iNoOfTexts++;
	}

	s32 virtualTotal = 0;
	s32 selectedVirtualTotal = 0;
	s32 selectedVirtualTotalWithGreaterThan1Ref = 0;
	int animDictCount = fwAnimManager::GetSize();

	if (m_showAnimDicts || m_showAnimMemory)
	{
		if (!sysBootManager::IsPackagedBuild())
		{
			if(g_ClipRpfBuildMetadata.GetClipDictionaryBuildMetadataCount() == 0)
			{
				atArray< atString > files;
				ASSET.EnumFiles("common://non_final/anim/clip_dictionary_metadata", FindClipDictionaryMetadataFilesCallback, &files);

				if(files.GetCount() > 0)
				{
					ASSET.PushFolder("common://non_final/anim/clip_dictionary_metadata");

					for(int i = 0; i < files.GetCount(); i ++)
					{
						fwClipRpfBuildMetadata clipRpfBuildMetadata;

						if(clipRpfBuildMetadata.Load(files[i]))
						{
							g_ClipRpfBuildMetadata.Merge(clipRpfBuildMetadata);
						}
					}

					ASSET.PopFolder();
				}
			}
		}

		atHashString selectedMemoryGroup(fwClipSetManager::GetMemoryGroupNames()[ms_iSelectedMemoryGroup]);
		for (int i = 0; i<animDictCount; i++)
		{
			// Is the current slot valid
			if (fwAnimManager::IsValidSlot(strLocalIndex(i)))
			{
				bool hasObjectLoaded    = false;
				bool isObjectInImage    = false;
				bool isObjectResident   = false;
				int refCount = fwAnimManager::GetNumRefs(strLocalIndex(i));
				int animCount = -1;

				if ( CStreaming::IsObjectInImage(strLocalIndex(i), fwAnimManager::GetStreamingModuleId()) )
				{
					isObjectInImage = true;
				}

				if ( CStreaming::HasObjectLoaded(strLocalIndex(i), fwAnimManager::GetStreamingModuleId()) )
				{
					hasObjectLoaded = true;
					animCount = fwAnimManager::CountAnimsInDict(i);
				}

				const char* p_dictName = fwAnimManager::GetName(strLocalIndex(i));

				const fwClipDictionaryMetadata *pClipDictionaryMetadata = fwClipSetManager::GetClipDictionaryMetadata(p_dictName);
				if (pClipDictionaryMetadata)
				{
					if (pClipDictionaryMetadata->GetStreamingPolicy() & SP_SINGLEPLAYER_RESIDENT)
					{
						isObjectResident = true;
					}
				}

				s32 virtualSize = 0;
				if ( isObjectInImage )
				{
					//virtualSize = CStreaming::GetObjectVirtualSize(i, fwAnimManager::GetStreamingModuleId())>>10;
					strIndex index = strStreamingEngine::GetInfo().GetModuleMgr().GetModule(fwAnimManager::GetStreamingModuleId())->GetStreamingIndex(strLocalIndex(i));					
					virtualSize = strStreamingEngine::GetInfo().GetObjectVirtualSize(index, true)>>10;
					virtualTotal += virtualSize;
				}

				if (   ( hasObjectLoaded  && m_isLoaded) 
					|| (!hasObjectLoaded  && m_isNotLoaded)

					|| ( isObjectResident && m_isResident)
					|| (!isObjectResident && m_isStreaming)

					|| ( (refCount>0)  && m_haveReferences)
					|| ( (refCount==0) && m_haveNoReferences) 					
					)
				{
					atHashString memoryGroup("MG_None");
					if (pClipDictionaryMetadata)
					{
						memoryGroup = pClipDictionaryMetadata->GetMemoryGroup();
						fwMemoryGroupMetadata* pMemoryGroupMetadata = fwClipSetManager::GetMemoryGroupMetadata(memoryGroup);
						if (pMemoryGroupMetadata)
						{
							pMemoryGroupMetadata->SetMemorySelected(pMemoryGroupMetadata->GetMemorySelected()+virtualSize);
						}
					}

					if (!m_filterByMemoryGroup || selectedMemoryGroup == memoryGroup)
					{
						sAnimDictSummary animNameGroupSize;

						m_selectedAnimDictSummary.push_back(animNameGroupSize);

						atVector<CAnimViewer::sAnimDictSummary>::reference group = m_selectedAnimDictSummary.back();
						group.pAnimDictName = p_dictName;
						group.animDictSize = virtualSize;
						group.animCount = animCount;
						group.refCount  = refCount;
						group.isResident = isObjectResident;  // resident or streaming (in the anim group manager)
						group.isInImage  = isObjectInImage;   // present in the anim image or missing
						group.hasLoaded  = hasObjectLoaded;	  // loaded in memory or unloaded
						group.memoryGroup = memoryGroup;

						if (refCount>1)
						{
							selectedVirtualTotalWithGreaterThan1Ref += virtualSize;
						}
						selectedVirtualTotal += virtualSize;
					}			
				}
			}
		}

		m_selectedCount = ptrdiff_t_to_int(m_selectedAnimDictSummary.size());

		if (m_selectedCount >0)
		{
			if (m_sortByName)
			{
				std::sort(&m_selectedAnimDictSummary[0], &m_selectedAnimDictSummary[0] + m_selectedAnimDictSummary.size(), AnimNameGroupSizeNameSort());
			}

			if (m_sortBySize)
			{
				std::sort(&m_selectedAnimDictSummary[0], &m_selectedAnimDictSummary[0] + m_selectedAnimDictSummary.size(), AnimNameGroupSizeSizeSort());
			}

			if (m_showAnimDicts)
			{
				int horizontalOffset = 0;

				// Calculate name column width

				atArray< atString > strings; strings.Reserve(32);
				strings.PushAndGrow(atString("Name"));
				int line = 0;
				for (atVector<sAnimDictSummary>::iterator group = m_selectedAnimDictSummary.begin(); group != m_selectedAnimDictSummary.end(); ++group)
				{
					line++;
					if (line>m_dictionaryScroll && line<(m_dictionaryScroll+30))
					{
						strings.PushAndGrow(atVarString("%s", group->pAnimDictName));
					}
				}
				int nameColumnWidth = GetMaxLength(strings) + 2;

				// Calculate size column width

				strings.Reset(); strings.Reserve(32);
				strings.PushAndGrow(atString("Size"));
				line = 0;
				for (atVector<sAnimDictSummary>::iterator group = m_selectedAnimDictSummary.begin(); group != m_selectedAnimDictSummary.end(); ++group)
				{
					line++;
					if (line>m_dictionaryScroll && line<(m_dictionaryScroll+30))
					{
						strings.PushAndGrow(atVarString("%d", group->animDictSize));
					}
				}
				int sizeColumnWidth = GetMaxLength(strings) + 2;

				// Calculate percentage of memory group column width

				strings.Reset(); strings.Reserve(32);
				strings.PushAndGrow(atString("%% MG"));
				line = 0;
				if (m_showPercentageOfMGColumn)
				{
					for (atVector<sAnimDictSummary>::iterator group = m_selectedAnimDictSummary.begin(); group != m_selectedAnimDictSummary.end(); ++group)
					{
						line++;
						if (line>m_dictionaryScroll && line<(m_dictionaryScroll+30))
						{
							fwMemoryGroupMetadata* pMemoryGroupMetadata = fwClipSetManager::GetMemoryGroupMetadata(group->memoryGroup);
							if (pMemoryGroupMetadata)
							{
								strings.PushAndGrow(atVarString("%.2f", ((f32)group->animDictSize/(f32)pMemoryGroupMetadata->GetMemorySelected())*100.0f));
							}
							else
							{
								strings.PushAndGrow(atVarString("?"));
							}
						}
					}
				}
				int percentageOfMGColumnWidth = GetMaxLength(strings) + 2;

				// Calculate percentage of memory group budget column width

				strings.Reset(); strings.Reserve(32);
				strings.PushAndGrow(atString("%% MG Budget"));
				line = 0;
				if (m_showPercentageOfMGBudgetColumn)
				{
					for (atVector<sAnimDictSummary>::iterator group = m_selectedAnimDictSummary.begin(); group != m_selectedAnimDictSummary.end(); ++group)
					{
						line++;
						if (line>m_dictionaryScroll && line<(m_dictionaryScroll+30))
						{
							fwMemoryGroupMetadata* pMemoryGroupMetadata = fwClipSetManager::GetMemoryGroupMetadata(group->memoryGroup);
							if (pMemoryGroupMetadata)
							{
								strings.PushAndGrow(atVarString("%.2f", ((f32)group->animDictSize/(f32)pMemoryGroupMetadata->GetMemoryBudget())*100.0f));
							}
							else
							{
								strings.PushAndGrow(atVarString("?"));
							}
						}
					}
				}
				int percentageOfMGBudgetColumnWidth = GetMaxLength(strings) + 2;

				// Calculate clip count column width

				strings.Reset(); strings.Reserve(32);
				strings.PushAndGrow(atString("# Clips"));
				line = 0;
				if (m_showClipCountColumn)
				{
					for (atVector<sAnimDictSummary>::iterator group = m_selectedAnimDictSummary.begin(); group != m_selectedAnimDictSummary.end(); ++group)
					{
						line++;
						if (line>m_dictionaryScroll && line<(m_dictionaryScroll+30))
						{
							strings.PushAndGrow(atVarString("%d", group->animCount));
						}
					}
				}
				int clipCountColumnWidth = GetMaxLength(strings) + 2;

				// Calculate loaded column width

				strings.Reset(); strings.Reserve(32);
				strings.PushAndGrow(atString("Loaded"));
				line = 0;
				if (m_showLoadedColumn)
				{
					for (atVector<sAnimDictSummary>::iterator group = m_selectedAnimDictSummary.begin(); group != m_selectedAnimDictSummary.end(); ++group)
					{
						line++;
						if (line>m_dictionaryScroll && line<(m_dictionaryScroll+30))
						{
							strings.PushAndGrow(atVarString("%s", group->hasLoaded ? "Y" : "N"));
						}
					}
				}
				int loadedColumnWidth = GetMaxLength(strings) + 2;

				// Calculate policy column width

				strings.Reset(); strings.Reserve(32);
				strings.PushAndGrow(atString("Policy"));
				line = 0;
				if (m_showPolicyColumn)
				{
					for (atVector<sAnimDictSummary>::iterator group = m_selectedAnimDictSummary.begin(); group != m_selectedAnimDictSummary.end(); ++group)
					{
						line++;
						if (line>m_dictionaryScroll && line<(m_dictionaryScroll+30))
						{
							strings.PushAndGrow(atVarString("%s", group->isResident ? "RESIDENT" : "STREAMING"));
						}
					}
				}
				int policyColumnWidth = GetMaxLength(strings) + 2;

				// Calculate reference count column width

				strings.Reset(); strings.Reserve(32);
				strings.PushAndGrow(atString("# Refs"));
				line = 0;
				if (m_showReferenceCountColumn)
				{
					for (atVector<sAnimDictSummary>::iterator group = m_selectedAnimDictSummary.begin(); group != m_selectedAnimDictSummary.end(); ++group)
					{
						line++;
						if (line>m_dictionaryScroll && line<(m_dictionaryScroll+30))
						{
							strings.PushAndGrow(atVarString("%d", group->refCount));
						}
					}
				}
				int referenceCountColumnWidth = GetMaxLength(strings) + 2;

				// Calculate memory group column width

				strings.Reset(); strings.Reserve(32);
				strings.PushAndGrow(atString("MemoryGroup"));
				line = 0;
				if (m_showMemoryGroupColumn)
				{
					for (atVector<sAnimDictSummary>::iterator group = m_selectedAnimDictSummary.begin(); group != m_selectedAnimDictSummary.end(); ++group)
					{
						line++;
						if (line>m_dictionaryScroll && line<(m_dictionaryScroll+30))
						{
							strings.PushAndGrow(atVarString("%s", group->memoryGroup.TryGetCStr()));
						}
					}
				}
				int memoryGroupColumnWidth = GetMaxLength(strings) + 2;

				// Calculate compression column width

				strings.Reset(); strings.Reserve(32);
				strings.PushAndGrow(atString("Compression %%"));
				line = 0;
				if (m_showCompressionColumn)
				{
					for (atVector<sAnimDictSummary>::iterator group = m_selectedAnimDictSummary.begin(); group != m_selectedAnimDictSummary.end(); ++group)
					{
						line++;
						if (line>m_dictionaryScroll && line<(m_dictionaryScroll+30))
						{
							const fwClipDictionaryBuildMetadata *pEntry = g_ClipRpfBuildMetadata.FindClipDictionaryBuildMetadata(group->pAnimDictName);
							if(pEntry)
							{
								strings.PushAndGrow(atVarString("%.2f%%", (static_cast< float >(pEntry->GetSizeAfter()) / static_cast< float >(pEntry->GetSizeBefore())) * 100.0f));
							}
							else
							{
								strings.PushAndGrow(atVarString("no data"));
							}
						}
					}
				}
				int compressionColumnWidth = GetMaxLength(strings) + 2;

				// Draw column headings

				sprintf(debugText, "Name");
				grcDebugDraw::PrintToScreenCoors(debugText, horizontalBorder + horizontalOffset,   verticalBorder + iNoOfTexts*vertDist, textColor);
				horizontalOffset += nameColumnWidth;

				sprintf(debugText, "Size");
				grcDebugDraw::PrintToScreenCoors(debugText, horizontalBorder + horizontalOffset, verticalBorder + iNoOfTexts*vertDist, textColor);
				horizontalOffset += sizeColumnWidth;
				
				if (m_showPercentageOfMGColumn)
				{
					sprintf(debugText, "%% MG");
					grcDebugDraw::PrintToScreenCoors(debugText, horizontalBorder + horizontalOffset, verticalBorder + iNoOfTexts*vertDist, textColor);
					horizontalOffset += percentageOfMGColumnWidth;
				}

				if (m_showPercentageOfMGBudgetColumn)
				{
					sprintf(debugText, "%% MG Budget");
					grcDebugDraw::PrintToScreenCoors(debugText, horizontalBorder + horizontalOffset, verticalBorder + iNoOfTexts*vertDist, textColor);
					horizontalOffset += percentageOfMGBudgetColumnWidth;
				}

				if (m_showClipCountColumn)
				{
					sprintf(debugText, "# Clips");
					grcDebugDraw::PrintToScreenCoors(debugText, horizontalBorder + horizontalOffset, verticalBorder + iNoOfTexts*vertDist, textColor);
					horizontalOffset += clipCountColumnWidth;
				}

				if(m_showLoadedColumn)
				{
					sprintf(debugText, "Loaded");
					grcDebugDraw::PrintToScreenCoors(debugText, horizontalBorder + horizontalOffset, verticalBorder + iNoOfTexts*vertDist, textColor);
					horizontalOffset += loadedColumnWidth;
				}

				if (m_showPolicyColumn)
				{
					sprintf(debugText, "Policy");
					grcDebugDraw::PrintToScreenCoors(debugText, horizontalBorder + horizontalOffset, verticalBorder + iNoOfTexts*vertDist, textColor);
					horizontalOffset += policyColumnWidth;
				}

				if (m_showReferenceCountColumn)
				{
					sprintf(debugText, "# Refs");
					grcDebugDraw::PrintToScreenCoors(debugText, horizontalBorder + horizontalOffset, verticalBorder + iNoOfTexts*vertDist, textColor);
					horizontalOffset += referenceCountColumnWidth;
				}

				if (m_showMemoryGroupColumn)
				{
					sprintf(debugText, "MemoryGroup");
					grcDebugDraw::PrintToScreenCoors(debugText, horizontalBorder + horizontalOffset, verticalBorder + iNoOfTexts*vertDist, textColor);
					horizontalOffset += memoryGroupColumnWidth;
				}

				if (m_showCompressionColumn)
				{
					sprintf(debugText, "Compression %%");
					grcDebugDraw::PrintToScreenCoors(debugText, horizontalBorder + horizontalOffset, verticalBorder + iNoOfTexts*vertDist, textColor);
					horizontalOffset += compressionColumnWidth;
				}

				iNoOfTexts++;
				iNoOfTexts++;

				// Draw lines

				line = 0;
				for (atVector<sAnimDictSummary>::iterator group = m_selectedAnimDictSummary.begin(); group != m_selectedAnimDictSummary.end(); ++group)
				{
					line++;
					
					if (line%2) 
					{
						textColor = Color_LawnGreen;
					}
					else
					{
						textColor = Color_RoyalBlue;
					}

					if (line>m_dictionaryScroll && line<(m_dictionaryScroll+30))
					{
						horizontalOffset = 0;

						// Name
						sprintf(debugText, "%s", group->pAnimDictName);
						grcDebugDraw::PrintToScreenCoors(debugText, horizontalBorder + horizontalOffset,   verticalBorder + iNoOfTexts*vertDist, textColor);
						horizontalOffset += nameColumnWidth;

						// Size
						sprintf(debugText, "%d", group->animDictSize);
						grcDebugDraw::PrintToScreenCoors(debugText, horizontalBorder + horizontalOffset, verticalBorder + iNoOfTexts*vertDist, textColor);
						horizontalOffset += sizeColumnWidth;

						fwMemoryGroupMetadata* pMemoryGroupMetadata = fwClipSetManager::GetMemoryGroupMetadata(group->memoryGroup);

						if (m_showPercentageOfMGColumn)
						{
							if (pMemoryGroupMetadata)
							{
								sprintf(debugText, "%.2f", ((f32)group->animDictSize/(f32)pMemoryGroupMetadata->GetMemorySelected())*100.0f);
							}
							else
							{
								sprintf(debugText, "?");
							}
							grcDebugDraw::PrintToScreenCoors(debugText, horizontalBorder + horizontalOffset, verticalBorder + iNoOfTexts*vertDist, textColor);
							horizontalOffset += percentageOfMGColumnWidth;
						}

						if (m_showPercentageOfMGBudgetColumn)
						{
							if (pMemoryGroupMetadata)
							{
								sprintf(debugText, "%.2f", ((f32)group->animDictSize/(f32)pMemoryGroupMetadata->GetMemoryBudget())*100.0f);
							}
							else
							{
								sprintf(debugText, "?");
							}
							grcDebugDraw::PrintToScreenCoors(debugText, horizontalBorder + horizontalOffset, verticalBorder + iNoOfTexts*vertDist, textColor);
							horizontalOffset += percentageOfMGBudgetColumnWidth;
						}

						if (m_showClipCountColumn)
						{
							// # clips
							sprintf(debugText, "%d", group->animCount);
							grcDebugDraw::PrintToScreenCoors(debugText, horizontalBorder + horizontalOffset, verticalBorder + iNoOfTexts*vertDist, textColor);
							horizontalOffset += clipCountColumnWidth;
						}

						if (m_showLoadedColumn)
						{
							// Loaded
							sprintf(debugText, "%s", group->hasLoaded ? "Y" : "N");
							grcDebugDraw::PrintToScreenCoors(debugText, horizontalBorder + horizontalOffset, verticalBorder + iNoOfTexts*vertDist, textColor);
							horizontalOffset += loadedColumnWidth;
						}

						if (m_showPolicyColumn)
						{
							// Streaming policy
							sprintf(debugText, "%s", group->isResident ? "RESIDENT" : "STREAMING");
							grcDebugDraw::PrintToScreenCoors(debugText, horizontalBorder + horizontalOffset, verticalBorder + iNoOfTexts*vertDist, textColor);
							horizontalOffset += policyColumnWidth;
						}

						if (m_showReferenceCountColumn)
						{
							// # references
							sprintf(debugText, "%d", group->refCount);
							grcDebugDraw::PrintToScreenCoors(debugText, horizontalBorder + horizontalOffset, verticalBorder + iNoOfTexts*vertDist, textColor);
							horizontalOffset += referenceCountColumnWidth;
						}

						if (m_showMemoryGroupColumn)
						{
							sprintf(debugText, "%s", group->memoryGroup.TryGetCStr());
							grcDebugDraw::PrintToScreenCoors(debugText, horizontalBorder + horizontalOffset, verticalBorder + iNoOfTexts*vertDist, textColor);
							horizontalOffset += memoryGroupColumnWidth;
						}

						if (m_showCompressionColumn)
						{
							const fwClipDictionaryBuildMetadata *pEntry = g_ClipRpfBuildMetadata.FindClipDictionaryBuildMetadata(group->pAnimDictName);
							if(pEntry)
							{
								sprintf(debugText, "%.2f%%", (static_cast< float >(pEntry->GetSizeAfter()) / static_cast< float >(pEntry->GetSizeBefore())) * 100.0f);
							}
							else
							{
								sprintf(debugText, "no data");
							}
							grcDebugDraw::PrintToScreenCoors(debugText, horizontalBorder + horizontalOffset, verticalBorder + iNoOfTexts*vertDist, textColor);
							horizontalOffset += compressionColumnWidth;
						}

						iNoOfTexts++;
					}
				}
			}
		}

#if ANIMASSOC_STATS
	//PF_INCREMENTBY(MG_None_Counter,	memoryGroupTotal[MG_None]);
	//PF_INCREMENTBY(MG_AmbientMovement_Counter,memoryGroupTotal[MG_AmbientMovement]);
	//PF_INCREMENTBY(MG_CivilianReactions_Counter,memoryGroupTotal[MG_CivilianReactions]);
	//PF_INCREMENTBY(MG_Combat_Counter,memoryGroupTotal[MG_Combat]);
	//PF_INCREMENTBY(MG_CoreMovement_Counter,memoryGroupTotal[MG_CoreMovement]);
	//PF_INCREMENTBY(MG_Cover_Counter,memoryGroupTotal[MG_Cover]);
	//PF_INCREMENTBY(MG_Creature_Counter,memoryGroupTotal[MG_Creature]);
	//PF_INCREMENTBY(MG_Facial_Counter,memoryGroupTotal[MG_Facial]);
	//PF_INCREMENTBY(MG_Gesture_Counter,memoryGroupTotal[MG_Gesture]);
	//PF_INCREMENTBY(MG_Melee_Counter,memoryGroupTotal[MG_Melee]);
	//PF_INCREMENTBY(MG_Misc_Counter,memoryGroupTotal[MG_Misc]);
	//PF_INCREMENTBY(MG_Parachuting_Counter,memoryGroupTotal[MG_Parachuting]);
	//PF_INCREMENTBY(MG_PlayerMovement_Counter,memoryGroupTotal[MG_PlayerMovement]);
	//PF_INCREMENTBY(MG_ScenariosBase_Counter,memoryGroupTotal[MG_ScenariosBase]);
	//PF_INCREMENTBY(MG_ScenariosStreamed_Counter,memoryGroupTotal[MG_ScenariosStreamed]);
	//PF_INCREMENTBY(MG_Script_Counter,memoryGroupTotal[MG_Script]);
	//PF_INCREMENTBY(MG_ShockingEventReactions_Counter,memoryGroupTotal[MG_ShockingEventReactions]);
	//PF_INCREMENTBY(MG_Swimming_Counter,memoryGroupTotal[MG_Swimming]);
	//PF_INCREMENTBY(MG_VaultingClimbingJumping_Counter,memoryGroupTotal[MG_VaultingClimbingJumping]);
	//PF_INCREMENTBY(MG_Vehicle_Counter,memoryGroupTotal[MG_Vehicle]);
	//PF_INCREMENTBY(MG_Weapons_Counter,memoryGroupTotal[MG_Weapons]);
	//PF_INCREMENTBY(CAnimAssoc_total,	selectedVirtualTotal);
#endif // ANIMASSOC_STATS

		m_selectedAnimDictSummary.clear();
	}

	if (m_showAnimMemory)
	{
		iNoOfTexts++;
		sprintf(debugText, "Animation memory groups\n");
		grcDebugDraw::PrintToScreenCoors(debugText, horizontalBorder + 0,   verticalBorder + iNoOfTexts*vertDist);
		iNoOfTexts++;
		for (int i=0; i<memoryGroupCount; i++)
		{			
			if (!m_filterByMemoryGroup || (m_filterByMemoryGroup && (ms_iSelectedMemoryGroup ==i) ))
			{
				if (i%2) 
				{
					textColor = Color_LawnGreen;
				}
				else
				{
					textColor = Color_RoyalBlue;
				}

				sprintf(debugText, "%s\n", fwClipSetManager::GetMemoryGroupNames()[i]);
				grcDebugDraw::PrintToScreenCoors(debugText, horizontalBorder + 0,  verticalBorder + iNoOfTexts*vertDist, textColor);

				bool bOverBudget = false;
				atHashString selectedMemoryGroup(fwClipSetManager::GetMemoryGroupNames()[i]);
				fwMemoryGroupMetadata* pMemoryGroupMetadata = fwClipSetManager::GetMemoryGroupMetadata(selectedMemoryGroup);
				if (pMemoryGroupMetadata)
				{
					float percentage = ((f32)pMemoryGroupMetadata->GetMemorySelected()/(f32)pMemoryGroupMetadata->GetMemoryBudget())*100.0f; 
					if (percentage>100.0f)
						bOverBudget = true;

					sprintf(debugText, "%d/%d=%f\n", 
						pMemoryGroupMetadata->GetMemorySelected(),
						pMemoryGroupMetadata->GetMemoryBudget(), percentage );
				}
				else
				{
					sprintf(debugText, "?\n");
				}

				int horizontalOffset = 3 * horizontalIncrement;

				if (bOverBudget)
					grcDebugDraw::PrintToScreenCoors(debugText, horizontalBorder + horizontalOffset, verticalBorder + iNoOfTexts*vertDist, Color_IndianRed);
				else
					grcDebugDraw::PrintToScreenCoors(debugText, horizontalBorder + horizontalOffset, verticalBorder + iNoOfTexts*vertDist, textColor);
				iNoOfTexts++;
			}
		}

		iNoOfTexts++;

		sprintf(debugText, "Selected anim dict count %d\n", m_selectedCount);
		grcDebugDraw::PrintToScreenCoors(debugText, horizontalBorder + 0,   verticalBorder + iNoOfTexts*vertDist);
		iNoOfTexts++;

		sprintf(debugText, "Selected virtual memory %dK\n", selectedVirtualTotal);
		grcDebugDraw::PrintToScreenCoors(debugText, horizontalBorder + 0,   verticalBorder + iNoOfTexts*vertDist);
		iNoOfTexts++;

		sprintf(debugText, "Selected virtual memory with > 1 refs %dK\n", selectedVirtualTotalWithGreaterThan1Ref);
		grcDebugDraw::PrintToScreenCoors(debugText, horizontalBorder + 0,   verticalBorder + iNoOfTexts*vertDist);
		iNoOfTexts++;

		iNoOfTexts++;

		sprintf(debugText, "Total anim dict count %d\n", animDictCount);
		grcDebugDraw::PrintToScreenCoors(debugText, horizontalBorder + 0,   verticalBorder + iNoOfTexts*vertDist);
		iNoOfTexts++;

		sprintf(debugText, "Total virtual memory %dK\n", virtualTotal);
		grcDebugDraw::PrintToScreenCoors(debugText, horizontalBorder + 0,   verticalBorder + iNoOfTexts*vertDist);
		iNoOfTexts++;
	}

	if (m_renderFrameFilterDictionaryDebug)
	{
		int horizontalOffset = 0;

		sprintf(debugText, "FrameFilterDictionaryStore");
		grcDebugDraw::PrintToScreenCoors(debugText, horizontalBorder + horizontalOffset,   verticalBorder + iNoOfTexts*vertDist, textColor);
		iNoOfTexts++;

		sprintf(debugText, "Name");
		grcDebugDraw::PrintToScreenCoors(debugText, horizontalBorder + horizontalOffset,   verticalBorder + iNoOfTexts*vertDist, textColor);
		horizontalOffset += 4 * horizontalIncrement;

		sprintf(debugText, "# Refs");
		grcDebugDraw::PrintToScreenCoors(debugText, horizontalBorder + horizontalOffset, verticalBorder + iNoOfTexts*vertDist, textColor);
		iNoOfTexts++;

		// Iterate through filter dictionaries
		for(int i = 0; i < g_FrameFilterDictionaryStore.GetCount(); i++)
		{
			horizontalOffset = 0;

			if(g_FrameFilterDictionaryStore.IsValidSlot(strLocalIndex(i)) && g_FrameFilterDictionaryStore.IsObjectInImage(strLocalIndex(i)))
			{						
				// Get filter dictionary
				crFrameFilterDictionary *frameFilterDictionary = g_FrameFilterDictionaryStore.Get(strLocalIndex(i));
				const char *frameFilterDictionaryName = g_FrameFilterDictionaryStore.GetName(strLocalIndex(i));
				if(frameFilterDictionary)
				{
					sprintf(debugText, "%s", frameFilterDictionaryName ? frameFilterDictionaryName : "?");
					grcDebugDraw::PrintToScreenCoors(debugText, horizontalBorder + horizontalOffset,   verticalBorder + iNoOfTexts*vertDist, textColor);
					horizontalOffset += 4 * horizontalIncrement;

					sprintf(debugText, "%d", g_FrameFilterDictionaryStore.GetNumRefs(strLocalIndex(i)));
					grcDebugDraw::PrintToScreenCoors(debugText, horizontalBorder + horizontalOffset, verticalBorder + iNoOfTexts*vertDist, textColor);
					iNoOfTexts++;
					iNoOfTexts++;

					horizontalOffset = 0;

					sprintf(debugText, "Index");
					grcDebugDraw::PrintToScreenCoors(debugText, horizontalBorder + horizontalOffset,   verticalBorder + iNoOfTexts*vertDist, textColor);
					horizontalOffset += horizontalIncrement;

					sprintf(debugText, "Name");
					grcDebugDraw::PrintToScreenCoors(debugText, horizontalBorder + horizontalOffset,   verticalBorder + iNoOfTexts*vertDist, textColor);
					horizontalOffset += 3 * horizontalIncrement;

					sprintf(debugText, "# Refs");
					grcDebugDraw::PrintToScreenCoors(debugText, horizontalBorder + horizontalOffset,   verticalBorder + iNoOfTexts*vertDist, textColor);
					iNoOfTexts++;


					// Iterate through filters
					for(int j = 0; j < frameFilterDictionary->GetCount(); j ++)
					{
						horizontalOffset = 0;

						crFrameFilter *pFrameFilter = frameFilterDictionary->GetEntry(j);
						const char *pFrameFilterName = atHashString(frameFilterDictionary->GetCode(j)).TryGetCStr();

						sprintf(debugText, "%d", j);
						grcDebugDraw::PrintToScreenCoors(debugText, horizontalBorder + horizontalOffset,   verticalBorder + iNoOfTexts*vertDist, textColor);
						horizontalOffset += horizontalIncrement;

						sprintf(debugText, "%s", pFrameFilterName ? pFrameFilterName : "?");
						grcDebugDraw::PrintToScreenCoors(debugText, horizontalBorder + horizontalOffset,   verticalBorder + iNoOfTexts*vertDist, textColor);
						horizontalOffset += 3 * horizontalIncrement;

						sprintf(debugText, "%d", pFrameFilter ? pFrameFilter->GetRef() : -1);
						grcDebugDraw::PrintToScreenCoors(debugText, horizontalBorder + horizontalOffset,   verticalBorder + iNoOfTexts*vertDist, textColor);
						iNoOfTexts++;
					}
				}

			}
		}
	}

	if (m_renderSynchronizedSceneDebug)
	{
		// render some debug information on synchronized scenes
		atString syncedSceneDebugInfo;

		fwAnimDirectorComponentSyncedScene::DumpSynchronizedSceneDebugInfo(syncedSceneDebugInfo);

		syncedSceneDebugInfo.Replace("\t", "    ");

		atArray<atString> list;
		syncedSceneDebugInfo.Split(list, "\n");

		const grcFont *pTextFont = grcFont::HasCurrent() ? &grcFont::GetCurrent() : nullptr;

		const int iTextFontWidth = pTextFont ? pTextFont->GetWidth() + 1 : 10;
		const int iTextFontHeight = pTextFont ? pTextFont->GetHeight() + 1 : 10;

		for (int i = 0; i < list.GetCount(); i++)
		{
			grcDebugDraw::Text((float)horizontalBorder + (m_renderSynchronizedSceneDebugHorizontalScroll * iTextFontWidth),
				(float)verticalBorder + ((m_renderSynchronizedSceneDebugVerticalScroll + i) * iTextFontHeight),
				DD_ePCS_Pixels, Color_white, list[i].c_str());
		}

		fwAnimDirectorComponentSyncedScene::RenderSynchronizedSceneDebug();
	}

	if (m_renderFacialVariationDebug)
	{
		Color32 colour(Color_red);
		char debugText[100];
		Vector2 vTextRenderPos(m_fHorizontalBorder, m_fVerticalBorder + (m_iNoOfTexts)*m_fVertDist);

		// Render some debug information about the facial variation trackers
		atString facialVariationDebugInfo;

		fwFacialClipSetGroupManager::DumpTrackerDebugInfo(facialVariationDebugInfo, m_renderFacialVariationTrackerCount);

		atArray<atString> list;
		facialVariationDebugInfo.Split(list, "\n");

		for (int i= 0; i< list.GetCount(); i++)
		{
			sprintf(debugText,"%s", list[i].c_str());
			vTextRenderPos.y = m_fVerticalBorder + (m_iNoOfTexts++)*m_fVertDist;
			grcDebugDraw::Text(vTextRenderPos, colour, debugText);
		}
}
}

void CAnimViewer::PrintMemoryUsageReport()
{
	m_selectedAnimDictSummary.clear();

	char 		pLine[255];
	FileHandle	fid;

	int memoryGroupCount = fwClipSetManager::GetMemoryGroupMetadataCount();
	for (int i=0; i<memoryGroupCount; i++)
	{			
		fwMemoryGroupMetadata* pMemoryGroupMetadata = fwClipSetManager::GetMemoryGroupMetadataByIndex(i);
		if (pMemoryGroupMetadata)
		{
			pMemoryGroupMetadata->SetMemorySelected(0);
		}
	}

	// Try and open a file for the results
	CFileMgr::SetDir("common:/DATA");
	fid = CFileMgr::OpenFileForWriting("AnimationReport.csv");
	CFileMgr::SetDir("");

	if ( !CFileMgr::IsValidFileHandle(fid) )
	{
		Displayf("\\common\\data\\AnimationReport.csv is read only");
		return;
	}

	sprintf(&pLine[0], 
		"Name, Size, Number of Anims, In image, Loaded, Number of references, Memory Group\n");

#if __DEV
	animAssert(CFileMgr::Write(fid, &pLine[0], istrlen(pLine))>0);
#else // __DEV
	CFileMgr::Write(fid, &pLine[0], istrlen(pLine));
#endif //__DEV

	s32 virtualTotal = 0;
	s32 selectedVirtualTotal = 0;

	int animDictCount = fwAnimManager::GetSize();
	for (int i = 0; i<animDictCount; i++)
	{
		// Is the current slot valid
		if (fwAnimManager::IsValidSlot(strLocalIndex(i)))
		{
			bool hasObjectLoaded    = false;
			bool isObjectInImage    = false;
			bool isObjectResident   = false;
			bool isObjectFromCode	= false;
			int refCount = fwAnimManager::GetNumRefs(strLocalIndex(i));
			int animCount = -1;

			if ( CStreaming::IsObjectInImage(strLocalIndex(i), fwAnimManager::GetStreamingModuleId()) )
			{
				isObjectInImage = true;
			}

			if ( CStreaming::HasObjectLoaded(strLocalIndex(i), fwAnimManager::GetStreamingModuleId()) )
			{
				hasObjectLoaded = true;
				animCount = fwAnimManager::CountAnimsInDict(i);
			}

			const char* p_dictName = fwAnimManager::GetName(strLocalIndex(i));
			const fwClipDictionaryMetadata *pClipDictionaryMetadata = fwClipSetManager::GetClipDictionaryMetadata(p_dictName);
			if (pClipDictionaryMetadata)
			{
				isObjectFromCode = true;
				if (pClipDictionaryMetadata->GetStreamingPolicy() & SP_SINGLEPLAYER_RESIDENT)
				{
					isObjectResident = true;
				}
			}

			s32 virtualSize = 0;
			//virtualSize = CStreaming::GetObjectVirtualSize(i, fwAnimManager::GetStreamingModuleId())>>10;
			strIndex index = strStreamingEngine::GetInfo().GetModuleMgr().GetModule(fwAnimManager::GetStreamingModuleId())->GetStreamingIndex(strLocalIndex(i));
			virtualSize = strStreamingEngine::GetInfo().GetObjectVirtualSize(index, true)>>10;
			virtualTotal += virtualSize;

			{

				sAnimDictSummary animNameGroupSize;
				m_selectedAnimDictSummary.push_back(animNameGroupSize);

				atVector<CAnimViewer::sAnimDictSummary>::reference group = m_selectedAnimDictSummary.back();
				group.pAnimDictName = p_dictName;
				group.animDictSize = virtualSize;
				group.animCount = animCount;
				group.refCount  = refCount;

				group.isResident = isObjectResident;  // resident or streaming (in the  anim group manager)
				group.isInImage  = isObjectInImage;   // present in the anim image or missing
				group.hasLoaded  = hasObjectLoaded;	  // loaded in memory or unloaded

				group.memoryGroup = atHashString("MG_None");
				if (pClipDictionaryMetadata)
				{
					group.memoryGroup = pClipDictionaryMetadata->GetMemoryGroup();
					fwMemoryGroupMetadata* pMemoryGroupMetadata = fwClipSetManager::GetMemoryGroupMetadata(group.memoryGroup);
					if (pMemoryGroupMetadata)
					{
						pMemoryGroupMetadata->SetMemorySelected(pMemoryGroupMetadata->GetMemorySelected()+virtualSize);
					}
				}
				selectedVirtualTotal += virtualSize;
			}
		}
	}

	m_selectedCount = ptrdiff_t_to_int(m_selectedAnimDictSummary.size());

	if (m_selectedCount >0)
	{
		if (m_sortByName)
		{
			std::sort(&m_selectedAnimDictSummary[0], &m_selectedAnimDictSummary[0] + m_selectedAnimDictSummary.size(), AnimNameGroupSizeNameSort());
		}

		if (m_sortBySize)
		{
			std::sort(&m_selectedAnimDictSummary[0], &m_selectedAnimDictSummary[0] + m_selectedAnimDictSummary.size(), AnimNameGroupSizeSizeSort());
		}

		for (atVector<sAnimDictSummary>::iterator group = m_selectedAnimDictSummary.begin(); group != m_selectedAnimDictSummary.end(); ++group)
		{
			sprintf(&pLine[0], "%s, %d, %d, %s, %s, %s, %d, %s\n",
				group->pAnimDictName, 
				group->animDictSize, 
				group->animCount, 
				group->isInImage ? "IN IMAGE" : "NOT IN IMAGE",
				group->hasLoaded ? "LOADED" : "NOT LOADED",
				//group->isFromCode ? "CODE" : "SCRIPT",
				group->isResident ? "RESIDENT" : "STREAMING",
				group->refCount,
				group->memoryGroup.TryGetCStr()
				);
#if __DEV
				animAssert(CFileMgr::Write(fid, &pLine[0], istrlen(pLine))>0);
#else // __DEV
				CFileMgr::Write(fid, &pLine[0], istrlen(pLine));
#endif // __DEV
		}

		sprintf(&pLine[0], "\n");

		sprintf(&pLine[0], "Anim memory groups\n");
		for (int i=0; i<memoryGroupCount; i++)
		{
			atHashString selectedMemoryGroup(fwClipSetManager::GetMemoryGroupNames()[i]);
			fwMemoryGroupMetadata* pMemoryGroupMetadata = fwClipSetManager::GetMemoryGroupMetadata(selectedMemoryGroup);
			if (pMemoryGroupMetadata)
			{
				sprintf(&pLine[0], "%s, %d\n", fwClipSetManager::GetMemoryGroupNames()[i], pMemoryGroupMetadata->GetMemorySelected());
#if __DEV
				animAssert(CFileMgr::Write(fid, &pLine[0], istrlen(pLine))>0);
#else // __DEV
				CFileMgr::Write(fid, &pLine[0], istrlen(pLine));
#endif // __DEV
			}
		}
	}

	CFileMgr::CloseFile(fid);
	animDisplayf("Saved \\common\\data\\AnimationReport.csv");
}

#if ENABLE_BLENDSHAPES
void CAnimViewer::LogBlendshapesAfterPose()
{
	crCreatureComponentBlendShapes::ms_iLogPose = 2;
}

void CAnimViewer::LogBlendshapesAfterFinalize()
{
	crCreatureComponentBlendShapes::ms_iLogFinalize  = 2;
}
#endif // ENABLE_BLENDSHAPES

void CAnimViewer::LogShaderVarAfterPose()
{
}

void CAnimViewer::LogShaderVarAfterDraw()
{
}

#if ENABLE_BLENDSHAPES
void CAnimViewer::PrintBlendshapeReport()
{
	if(!m_pDynamicEntity || !m_pDynamicEntity->GetIsTypePed())
		return;

	CPed* pPed = static_cast<CPed*>(m_pDynamicEntity.Get());
	if (!pPed)
	{
		return;
	}

	rmcDrawable* pHeadComponent = pPed->GetComponent(PV_COMP_HEAD);
	if (pHeadComponent)
	{
		animDisplayf("Start Blendshape report\n");
		grbTargetManager* pBlendShapeManager = m_pDynamicEntity->GetTargetManager();
		if(pBlendShapeManager)
		{
			animDisplayf("grbBlendShapes and grbMorphables");

			animDisplayf("Blendshape count = %d", pBlendShapeManager->GetBlendShapeCount());
			
			for (int i = 0; i < pBlendShapeManager->GetBlendShapeCount(); i++)
			{
				grbBlendShape* pBlendShape = pBlendShapeManager->GetBlendShape(i);
				Assert(pBlendShape);
				if (pBlendShape)
				{
					animDisplayf("Blendshape (%d) = %s -> Morphable count = %d", i, pBlendShape->GetName(), pBlendShape->GetMorphableCount());
					for (int j = 0; j < pBlendShape->GetMorphableCount(); j++)
					{
						grbMorphable* pMorphable = pBlendShape->GetMorphable(j);
						Assert(pMorphable);
						if (pMorphable)
						{
							animDisplayf("Morphable (%d) = %s -> Vertex count = %d", j, pMorphable->GetName(), pMorphable->GetVertexCount());							
						}
					}
				}
			}

			animDisplayf("grbTargets");
			atMap<u16, datOwner<grbTarget> >::ConstIterator iter = pBlendShapeManager->GetBlendTargetIterator();
			while(iter)
			{
				const grbTarget *pTarget = iter.GetData();
				if (pTarget)
				{		
					animDisplayf("key = %d,  name = %s", iter.GetKey(), pTarget->GetName());
				}
				++iter;
			}
		}
		
		animDisplayf("End Blendshape report\n");

	}
}
#endif // ENABLE_BLENDSHAPES

// Log all the anims
void CAnimViewer::LogAllAnims()
{
	char 		pLine[255];
	FileHandle	fid;

	bool fileOpen = false;

	// Try and open a file for the results
	CFileMgr::SetDir("common:/DATA");
	fid = CFileMgr::OpenFileForWriting("LogAllAnimations.csv");
	CFileMgr::SetDir("");

	if ( !CFileMgr::IsValidFileHandle(fid) )
	{
		Displayf("\\common\\data\\LogAllAnimations.csv is read only");
		return;
	}

	fileOpen = true;
	if (m_bLogAnimDictIndexAndHashKeys)
	{
		Displayf("dict name, anim name, dict streaming index, anim hash key\n");
	}
	else
	{
		Displayf("dict name, anim name\n");
	}

	//sprintf(&pLine[0], "dict name, anim name, dict streaming index, anim hash key\n");
	//ReturnVal = CFileMgr::Write(fid, &pLine[0], sizeof(char) * strlen(pLine));
	//animAssert(ReturnVal > 0);

	int nValidAnimCount = 0;
	int nInValidAnimCount = 0;
	int animDictCount = fwAnimManager::GetSize();
	for (int i = 0; i<animDictCount; i++)
	{
		// Is the current slot valid
		if (fwAnimManager::IsValidSlot(strLocalIndex(i)))
		{
			if ( CStreaming::IsObjectInImage(strLocalIndex(i), fwAnimManager::GetStreamingModuleId()) )
			{
				CStreaming::RequestObject(strLocalIndex(i), fwAnimManager::GetStreamingModuleId(), STRFLAG_FORCE_LOAD);
				CStreaming::LoadAllRequestedObjects();
				if ( CStreaming::HasObjectLoaded(strLocalIndex(i), fwAnimManager::GetStreamingModuleId()) )
				{
					int animCount = fwAnimManager::CountAnimsInDict(i);
					for (int j = 0; j<animCount; j++)
					{
						const crClip *pClip = fwAnimManager::GetClipInDict(i, j);
						animAssert(pClip);
						if (pClip)
						{			
							if (fileOpen)
							{
								if (m_bLogAnimDictIndexAndHashKeys)
								{
									sprintf(&pLine[0], "%s, %s, %d, %d \n", fwAnimManager::GetName(strLocalIndex(i)), pClip->GetName(), i, fwAnimManager::GetAnimHashInDict(i, j));
									Displayf("%s, %s, %d, %d \n", fwAnimManager::GetName(strLocalIndex(i)), pClip->GetName(), i, fwAnimManager::GetAnimHashInDict(i, j));
								}
								else
								{
									sprintf(&pLine[0], "%s, %s\n", fwAnimManager::GetName(strLocalIndex(i)), pClip->GetName());
									Displayf("%s, %s\n", fwAnimManager::GetName(strLocalIndex(i)), pClip->GetName());
								}
								#if __DEV
									animAssert(CFileMgr::Write(fid, &pLine[0], istrlen(pLine))>0);
								#else // __DEV
									CFileMgr::Write(fid, &pLine[0], istrlen(pLine));
								#endif // __DEV
							}

							nValidAnimCount++;

						}
						else
						{
							nInValidAnimCount++; 
							//Displayf("Invalid anim -> %d, %d\n", i, j);
						}
					}
				}
			}
		}
	}

	CFileMgr::CloseFile(fid);
	animDisplayf("Saved \\common\\data\\LogAllAnimations.csv");
}

void CAnimViewer::LogAllExpressions()
{
	char 		pLine[255];
	FileHandle	fid;

	bool fileOpen = false;

	// Try and open a file for the results
	CFileMgr::SetDir("common:/DATA");
	fid = CFileMgr::OpenFileForWriting("LogAllExpressions.csv");
	CFileMgr::SetDir("");
	if ( CFileMgr::IsValidFileHandle(fid) )
	{
		fileOpen = true;
		Displayf("dict name, expressions name\n");
		//sprintf(&pLine[0], "dict name, anim name, dict streaming index, anim hash key\n");
		//ReturnVal = CFileMgr::Write(fid, &pLine[0], sizeof(char) * strlen(pLine));
		//animAssert(ReturnVal > 0);
	}
	else
	{
		Displayf("LogAllExpressions.csv is read only");
		grcDebugDraw::PrintToScreenCoors("LogAllExpressions.csv is read only", 5, 6);
	}

	int nValidAnimCount = 0;
	int nInValidAnimCount = 0;
	int animDictCount = g_ExpressionDictionaryStore.GetCount();
	for (int i = 0; i<animDictCount; i++)
	{
		// Is the current slot valid
		if (g_ExpressionDictionaryStore.IsValidSlot(strLocalIndex(i)))
		{
			if ( CStreaming::IsObjectInImage(strLocalIndex(i), g_ExpressionDictionaryStore.GetStreamingModuleId()) )
			{
				CStreaming::RequestObject(strLocalIndex(i), g_ExpressionDictionaryStore.GetStreamingModuleId(), STRFLAG_FORCE_LOAD);
				CStreaming::LoadAllRequestedObjects();
				if ( CStreaming::HasObjectLoaded(strLocalIndex(i), g_ExpressionDictionaryStore.GetStreamingModuleId()) )
				{
					int animCount = g_ExpressionDictionaryStore.Get(strLocalIndex(i))->GetCount();
					for (int j = 0; j<animCount; j++)
					{
						const crExpressions *pClip = g_ExpressionDictionaryStore.Get(strLocalIndex(i))->Lookup(g_ExpressionDictionaryStore.Get(strLocalIndex(i))->GetCode(j));
						animAssert(pClip);
						if (pClip)
						{			
							if (fileOpen)
							{
								sprintf(&pLine[0], "%s, %s\n", g_ExpressionDictionaryStore.GetName(strLocalIndex(i)), pClip->GetName());
								Displayf("%s, %s\n", g_ExpressionDictionaryStore.GetName(strLocalIndex(i)), pClip->GetName());
#if __DEV
								animAssert(CFileMgr::Write(fid, &pLine[0], sizeof(char) * istrlen(pLine))>0);
#else
								CFileMgr::Write(fid, &pLine[0], sizeof(char) * istrlen(pLine));
#endif //__DEV
							}

							nValidAnimCount++;

						}
						else
						{
							nInValidAnimCount++; 
							//Displayf("Invalid anim -> %d, %d\n", i, j);
						}
					}
				}
			}
		}
	}

	if ( CFileMgr::IsValidFileHandle(fid) )
		CFileMgr::CloseFile(fid);
}

void CAnimViewer::RenderSkeleton(const crSkeleton& skeleton, Color32 boneColor, Color32 jointColor, float jointRadius)
{
	Vector3 a, b;
	const crSkeletonData &skeletonData = skeleton.GetSkeletonData();
	int totalBones = skeletonData.GetNumBones();
	for (int boneIndex = 0; boneIndex < totalBones; boneIndex++)
	{
		
		Matrix34 mBone;
		const crBoneData *boneData = skeletonData.GetBoneData(boneIndex);
		skeleton.GetGlobalMtx(boneIndex, RC_MAT34V(mBone));
		a.Set(mBone.d);
		for (const crBoneData *child = boneData->GetChild(); child; child = child->GetNext())
		{
			
			skeleton.GetGlobalMtx(child->GetIndex(), RC_MAT34V(mBone));
			b.Set(mBone.d);
			grcDebugDraw::Line(a, b, boneColor);
		}
	}

	//grcColor(Color_green);
	for (int boneIndex = 0; boneIndex < totalBones; boneIndex++)
	{
		const crBoneData *boneData = skeletonData.GetBoneData(boneIndex);

		//grcDrawSphere(0.005f, bone.GetGlobalMtx().d, 8, false, true);
		
		Matrix34 mBone;
		skeleton.GetGlobalMtx(boneIndex, RC_MAT34V(mBone));

		grcDebugDraw::Sphere(mBone.d, jointRadius, jointColor, true);

		for (const crBoneData *child = boneData->GetChild(); child; child = child->GetNext())
		{
			
			skeleton.GetGlobalMtx(child->GetIndex(), RC_MAT34V(mBone));

			//grcDrawSphere(0.005f, bone.GetGlobalMtx().d, 8, false, true);
			grcDebugDraw::Sphere(mBone.d, jointRadius, jointColor, true);
		}
	}
}

void CAnimViewer::ReadHashKeyFile()
{
	// Try and open the file for reading
	FileHandle inputFileHandle;
	const char* seps = " ,\t()";
	char* pToken;
	char* pLine;
	bool bInputFileOpen = false;

	CFileMgr::SetDir("common:/DATA");
	inputFileHandle = CFileMgr::OpenFile("HashKey.txt");
	animAssert(inputFileHandle > 0);
	s32 iLine = 0;

	atArray<u32> nHashKeys;

	if (CFileMgr::IsValidFileHandle(inputFileHandle))
	{
		bInputFileOpen = true;

		// Read in the list of hash keys
		while ((pLine = CFileLoader::LoadLine(inputFileHandle)) != NULL)
		{
			++iLine;
			if(*pLine == '#' || *pLine == '\0')
			{
				continue;
			}
			// Anim dict name
			pToken = strtok(pLine, seps);
			u32 nHashKey = 0;
			sscanf(pToken, "%d", &nHashKey);
			nHashKeys.PushAndGrow(nHashKey);
			continue;
		}
	}
	else
	{
		Displayf("Failed to open HashKey.txt for reading");
		grcDebugDraw::PrintToScreenCoors("Failed to open HashKey.txt for reading", 5, 6);
	}

	if (bInputFileOpen)
	{
		CFileMgr::CloseFile(inputFileHandle);
	}

	// Try and open a file for writing
	FileHandle outputFileHandle;
	bool bOutputFileOpen = false;
	char pOutputLine[255];
	CFileMgr::SetDir("common:/DATA");
	outputFileHandle = CFileMgr::OpenFileForWriting("ConvertedHashKeys.csv");
	if ( CFileMgr::IsValidFileHandle(outputFileHandle) )
	{
		bOutputFileOpen = true;
		//sprintf(&pLine[0], 
		//	"anim dict, anim name\n");
		//ReturnVal = CFileMgr::Write(outputFileHandle, &pLine[0], sizeof(char) * strlen(pLine));
		//animAssert(ReturnVal > 0);
	}
	else
	{
		Displayf("Failed to open ConvertedHashKeys.csv for writing");
		grcDebugDraw::PrintToScreenCoors("Failed to open ConvertedHashKeys.csv for writing", 5, 6);
	}


	// Check each hash key against each registered anim
	// For each hash key print a list of anim dict and anim names
	for (int k=0; k<nHashKeys.GetCount(); k++)
	{
		u32 nHashKey = nHashKeys[k];
		int nMatchCount = 0; 
		int animDictCount = fwAnimManager::GetSize();
		for (int i = 0; i<animDictCount; i++)
		{
			// Is the current slot valid
			if (fwAnimManager::IsValidSlot(strLocalIndex(i)))
			{
				if ( CStreaming::IsObjectInImage(strLocalIndex(i), fwAnimManager::GetStreamingModuleId()) )
				{
					CStreaming::RequestObject(strLocalIndex(i), fwAnimManager::GetStreamingModuleId(), STRFLAG_FORCE_LOAD);
					CStreaming::LoadAllRequestedObjects();
					if ( CStreaming::HasObjectLoaded(strLocalIndex(i), fwAnimManager::GetStreamingModuleId()) )
					{
						int animCount = fwAnimManager::CountAnimsInDict(i);
						for (int j = 0; j<animCount; j++)
						{
							const crClip *pClip = fwAnimManager::GetClipInDict(i, j);
							animAssert(pClip);
							if (pClip)
							{			
								if (fwAnimManager::GetAnimHashInDict(i, j) == nHashKey)
								{
									nMatchCount++;
									Displayf("%d = %s, %s\n", nHashKey, fwAnimManager::GetName(strLocalIndex(i)), pClip->GetName());
									if (bOutputFileOpen)
									{
										sprintf(&pOutputLine[0], "%s, %s\n", fwAnimManager::GetName(strLocalIndex(i)), pClip->GetName());
									#if __DEV
										animAssert(CFileMgr::Write(outputFileHandle, &pOutputLine[0], istrlen(pOutputLine))>0);
									#else // __DEV
										CFileMgr::Write(outputFileHandle, &pOutputLine[0], istrlen(pOutputLine));
									#endif // __DEV

									}
								}
							}
						}
					}
				}
			}
		}

		if (nMatchCount == 0)
		{
			Displayf("%d = No matches\n", nHashKey);			
		}

	}

	if (bOutputFileOpen)
	{
		CFileMgr::CloseFile(outputFileHandle);
	}
}

void CAnimViewer::UpdateAnimatedBuilding()
{
	CEntity *pEntity = static_cast< CEntity * >(g_PickerManager.GetSelectedEntity());
	if(pEntity && pEntity->GetIsTypeAnimatedBuilding())
	{
		CAnimatedBuilding *pAnimatedBuilding = static_cast< CAnimatedBuilding * >(pEntity);
		CMoveAnimatedBuilding &moveAnimatedBuilding = pAnimatedBuilding->GetMoveAnimatedBuilding();

		float rate = moveAnimatedBuilding.GetRate();
		if(rate != -1.0f)
		{
			ms_AnimatedBuildingRate = Clamp(rate, 0.0f, 2.0f);

			static const fwMvFloatId rateFloatId("PlaybackRateOut",0x83EF6397);

			atArray< fwMvId > outputParameters;
			outputParameters.Reserve(1);
			outputParameters.Append() = rateFloatId;

			moveAnimatedBuilding.ClearOutputParameters(outputParameters);
		}

		float phase = moveAnimatedBuilding.GetPhase();
		if(phase != -1.0f)
		{
			ms_AnimatedBuildingPhase = Clamp(phase, 0.0f, 1.0f);

			static const fwMvFloatId phaseFloatId("PlaybackPhaseOut",0x7CEE93A4);

			atArray< fwMvId > outputParameters;
			outputParameters.Reserve(1);
			outputParameters.Append() = phaseFloatId;

			moveAnimatedBuilding.ClearOutputParameters(outputParameters);
		}
	}
	else
	{
		ms_AnimatedBuildingRate = 0.0f;
		ms_AnimatedBuildingPhase = 0.0f;
	}
}

float CAnimViewer::ms_AnimatedBuildingRate = 1.0f;

void CAnimViewer::AnimatedBuildingAnimRateChanged()
{
	CEntity *pEntity = static_cast< CEntity * >(g_PickerManager.GetSelectedEntity());
	if(pEntity && pEntity->GetIsTypeAnimatedBuilding())
	{
		CAnimatedBuilding *pAnimatedBuilding = static_cast< CAnimatedBuilding * >(pEntity);

		scrVector scrVecCentreCoors = pAnimatedBuilding->GetTransform().GetPosition();
		float Radius = pAnimatedBuilding->GetBoundRadius();

		s32 ModelHashKey = pAnimatedBuilding->GetBaseModelInfo()->GetModelNameHash();

		object_commands::CommandSetNearestBuildingAnimRate(scrVecCentreCoors, Radius, ModelHashKey, ms_AnimatedBuildingRate, 0);
	}
}

void CAnimViewer::RandomAnimatedBuildingAnimRate()
{
	CEntity *pEntity = static_cast< CEntity * >(g_PickerManager.GetSelectedEntity());
	if(pEntity && pEntity->GetIsTypeAnimatedBuilding())
	{
		CAnimatedBuilding *pAnimatedBuilding = static_cast< CAnimatedBuilding * >(pEntity);

		scrVector scrVecCentreCoors = pAnimatedBuilding->GetTransform().GetPosition();
		float Radius = pAnimatedBuilding->GetBoundRadius();

		s32 ModelHashKey = pAnimatedBuilding->GetBaseModelInfo()->GetModelNameHash();

		object_commands::CommandSetNearestBuildingAnimRate(scrVecCentreCoors, Radius, ModelHashKey, 0.0f, object_commands::AB_RATE_RANDOM);
	}
}

float CAnimViewer::ms_AnimatedBuildingPhase = 0.0f;

void CAnimViewer::AnimatedBuildingAnimPhaseChanged()
{
	CEntity *pEntity = static_cast< CEntity * >(g_PickerManager.GetSelectedEntity());
	if(pEntity && pEntity->GetIsTypeAnimatedBuilding())
	{
		CAnimatedBuilding *pAnimatedBuilding = static_cast< CAnimatedBuilding * >(pEntity);

		scrVector scrVecCentreCoors = pAnimatedBuilding->GetTransform().GetPosition();
		float Radius = pAnimatedBuilding->GetBoundRadius();

		s32 ModelHashKey = pAnimatedBuilding->GetBaseModelInfo()->GetModelNameHash();

		object_commands::CommandSetNearestBuildingAnimPhase(scrVecCentreCoors, Radius, ModelHashKey, ms_AnimatedBuildingPhase, 0);
	}
}

void CAnimViewer::RandomAnimatedBuildingAnimPhase()
{
	CEntity *pEntity = static_cast< CEntity * >(g_PickerManager.GetSelectedEntity());
	if(pEntity && pEntity->GetIsTypeAnimatedBuilding())
	{
		CAnimatedBuilding *pAnimatedBuilding = static_cast< CAnimatedBuilding * >(pEntity);

		scrVector scrVecCentreCoors = pAnimatedBuilding->GetTransform().GetPosition();
		float Radius = pAnimatedBuilding->GetBoundRadius();

		s32 ModelHashKey = pAnimatedBuilding->GetBaseModelInfo()->GetModelNameHash();

		object_commands::CommandSetNearestBuildingAnimPhase(scrVecCentreCoors, Radius, ModelHashKey, 0.0f, object_commands::AB_PHASE_RANDOM);
	}
}

void CAnimViewer::UpdatePreviewCamera()
{
	if(ms_bUsingPreviewCamera)
	{
		if(!ms_bUsePreviewCamera)
		{
			// Turn preview camera off
			camInterface::GetDebugDirector().DeactivateFreeCam();

			ms_bUsingPreviewCamera = false;
		}
	}
	else
	{
		if(ms_bUsePreviewCamera)
		{
			// Turn preview camera on
			camInterface::GetDebugDirector().ActivateFreeCam();

			ms_bUsingPreviewCamera = true;
		}
	}

	if(ms_bUsingPreviewCamera)
	{
		// Update preview camera position and orientation

		CPed *pLocalPlayer = CPedFactory::GetFactory()->GetLocalPlayer();
		if(pLocalPlayer)
		{
			camFrame &freeCamFrame = camInterface::GetDebugDirector().GetFreeCamFrameNonConst();

			Vector3 vFront(0.0f, 1.0f, 0.0f);
			vFront.RotateZ(pLocalPlayer->GetCurrentHeading() + PI);
			Vector3 vUp(0.0f, 0.0f, 1.0f);
			freeCamFrame.SetWorldMatrixFromFrontAndUp(vFront, vUp);

			Vector3 vPosition;
			switch(ms_iPreviewCameraIndex)
			{
			case 0: // Full
				{
					vPosition = Vector3(0.0f, 2.6f, -0.1f);
				} break;
			case 1: // Three Quarter
				{
					vPosition = Vector3(0.0f, 1.75f, 0.25f);
				} break;
			case 2: // Head
				{
					vPosition = Vector3(0.0f, 0.70f, 0.60f);
				} break;
			}
			vPosition.RotateZ(pLocalPlayer->GetCurrentHeading());
			vPosition += pLocalPlayer->GetPreviousPosition();
			freeCamFrame.SetPosition(vPosition);

			ms_bUsingPreviewCamera = true;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
//	Move network previewing methods
//////////////////////////////////////////////////////////////////////////

void CAnimViewer::LoadNetworkButton()
{
	const char *szFilename = m_moveFileSelector.GetSelectedFilePath();
	if(Verifyf(strlen(szFilename) > 0, "Network filename is empty!"))
	{
		atArray<atString> networkSplit; atString(szFilename).Split(networkSplit, '.');
		if(Verifyf(networkSplit.GetCount() > 1, "Network filename does not have an extension!"))
		{
			if(Verifyf(ASSET.Exists(networkSplit[0], networkSplit[1]), "Network file does not exist!"))
			{
				LoadNetworkFromFile(szFilename);
			}
		}
	}
}

void CAnimViewer::LoadNetworkFromFile(const char * DEV_ONLY(filePath))
{
#if __DEV
	//get rid of any existing preview networks on the ped
	RemoveCurrentPreviewNetwork();

	if (m_pDynamicEntity)
	{
		if (m_pDynamicEntity->GetIsTypePed())
		{
			CPed* pPed = static_cast<CPed*>(m_pDynamicEntity.Get());
			if (pPed)
			{
				if (CTaskPreviewMoveNetwork::sm_secondary)
				{
					pPed->GetPedIntelligence()->AddTaskSecondary(rage_new CTaskPreviewMoveNetwork(filePath), PED_TASK_SECONDARY_PARTIAL_ANIM);
				}
				else
				{
					pPed->GetPedIntelligence()->AddTaskAtPriority(rage_new CTaskPreviewMoveNetwork(filePath), PED_TASK_PRIORITY_PRIMARY , true);	
				}
			}
		}
		else if (m_pDynamicEntity->GetIsTypeObject())
		{
			CObject* pObj = static_cast<CObject*>(m_pDynamicEntity.Get()); 
			if (pObj)
			{
				if (CTaskPreviewMoveNetwork::sm_secondary)
				{
					pObj->SetTask(rage_new CTaskPreviewMoveNetwork(filePath), CObjectIntelligence::OBJECT_TASK_TREE_SECONDARY, CObjectIntelligence::OBJECT_TASK_SECONDARY_ANIM);
				}
				else
				{
					pObj->SetTask(rage_new CTaskPreviewMoveNetwork(filePath), CObjectIntelligence::OBJECT_TASK_TREE_PRIMARY, CObjectIntelligence::OBJECT_TASK_PRIORITY_PRIMARY);
				}
			}
		}
		else if (m_pDynamicEntity->GetIsTypeVehicle())
		{
			CVehicle* pVeh = static_cast<CVehicle*>(m_pDynamicEntity.Get()); 
			if (pVeh)
			{
				pVeh->GetIntelligence()->GetTaskManager()->GetTree(VEHICLE_TASK_TREE_SECONDARY)->SetTask(rage_new CTaskPreviewMoveNetwork(filePath), VEHICLE_TASK_SECONDARY_ANIM); 
			}
		}
	}

#endif //__DEV
}

void CAnimViewer::RunMoVETestTask()
{
	//get rid of any existing preview networks on the ped
	RemoveCurrentPreviewNetwork();

	if (m_pDynamicEntity)
	{
		if (m_pDynamicEntity->GetIsTypePed())
		{
			CPed* pPed = static_cast<CPed*>(m_pDynamicEntity.Get());
			if (pPed)
			{
				pPed->GetPedIntelligence()->AddTaskAtPriority(rage_new CTaskTestMoveNetwork(), PED_TASK_PRIORITY_PRIMARY , true);	
			}
		}
	}
}

void CAnimViewer::StopMoVETestTask()
{
	if (m_pDynamicEntity)
	{
		if (m_pDynamicEntity->GetIsTypePed())
		{
			CPed* pPed = static_cast<CPed*>(m_pDynamicEntity.Get());
			if (pPed)
			{
				pPed->GetPedIntelligence()->ClearTasks(true);	
			}
		}
	}
}

void CAnimViewer::ReloadAllNetworks()
{
	fwAnimDirector::ShutdownStatic();
	fwAnimDirector::InitStatic();
}

//////////////////////////////////////////////////////////////////////////

void CAnimViewer::RemoveCurrentPreviewNetwork()
{
	//ClearParamWidgets();
	//CAnimNetwork::RemoveCurrentPreviewNetwork();
	if (m_pDynamicEntity)
	{
		if (m_pDynamicEntity->GetIsTypePed())
		{
			CPed* pPed = static_cast<CPed*>(m_pDynamicEntity.Get());
			if (pPed)
			{
				// get the anim preview task
				CTaskPreviewMoveNetwork* pTask = FindMovePreviewTask();

				if (pTask)
				{
					pTask->MakeAbortable(aiTask::ABORT_PRIORITY_IMMEDIATE, NULL);
				}
				else
				{
					// must be a secondary task
					pPed->GetPedIntelligence()->ClearTasks(false, true);
				}
			}
		}
		else if (m_pDynamicEntity->GetIsTypeObject())
		{
			CObject* pObj = static_cast<CObject*>(m_pDynamicEntity.Get());
			if (pObj && pObj->GetObjectIntelligence() && pObj->GetObjectIntelligence()->GetTaskManager())
			{
				if (CTaskPreviewMoveNetwork::sm_secondary)
				{
					pObj->GetObjectIntelligence()->GetTaskManager()->ClearTask(CObjectIntelligence::OBJECT_TASK_TREE_SECONDARY, CObjectIntelligence::OBJECT_TASK_SECONDARY_ANIM);
				}
				else
				{
					pObj->GetObjectIntelligence()->GetTaskManager()->ClearTask(CObjectIntelligence::OBJECT_TASK_TREE_PRIMARY, CObjectIntelligence::OBJECT_TASK_PRIORITY_PRIMARY);
				}
			}
		}
		else if (m_pDynamicEntity->GetIsTypeVehicle())
		{
			CVehicle* pVeh = static_cast<CVehicle*>(m_pDynamicEntity.Get());
			if (pVeh && pVeh->GetIntelligence() && pVeh->GetIntelligence()->GetTaskManager())
			{
				pVeh->GetIntelligence()->GetTaskManager()->ClearTask(VEHICLE_TASK_TREE_SECONDARY, VEHICLE_TASK_SECONDARY_ANIM);
			}
		}
	}
}

void CAnimViewer::AddParamWidgetsFromEntity(CDynamicEntity* pDynamicEntity)
{
	if (pDynamicEntity && m_previewNetworkGroup)
	{
		// get the anim preview task
		CTaskPreviewMoveNetwork* pTask = FindMovePreviewTask();

		if (pTask)
		{				
			m_pBank->SetCurrentGroup(*m_previewNetworkGroup);
			pTask->AddWidgets(m_pBank);
			m_pBank->UnSetCurrentGroup(*m_previewNetworkGroup);					
		}
	}
}

void CAnimViewer::RemoveParamWidgetsOnEntity(CDynamicEntity* pDynamicEntity)
{
	if (pDynamicEntity)
	{
		// get the anim preview task
		CTaskPreviewMoveNetwork* pTask = FindMovePreviewTask();

		if (pTask)
		{				
			pTask->RemoveWidgets();
		}
	}
}

void CAnimViewer::RequestParamWidgetOptionsRebuild()
{
	m_bNeedsParamWidgetOptionsRebuild = true;
}

void CAnimViewer::RebuildParamWidgetOptions()
{
	m_pBank->SetCurrentGroup(*m_widgetOptionsGroup);
	while (m_widgetOptionsGroup->GetChild())
	{
			m_pBank->Remove(*(m_widgetOptionsGroup->GetChild()));
	}
	
	switch((CMoveParameterWidget::ParameterWidgetType) m_newParamType)
	{
	case CMoveParameterWidget::kParameterWidgetReal:
		{
			m_pBank->AddText("Min value", &m_paramWidgetMinValue);
			m_pBank->AddText("Max value", &m_paramWidgetMaxValue);
			m_pBank->AddCombo("Pad control", &m_paramWidgetAnalogueControlElement, 7, &m_analogueControlElementNames[0]);
			m_pBank->AddToggle("Send every frame", &m_paramWidgetUpdateEveryFrame);
			m_pBank->AddToggle("Output mode", &m_paramWidgetOutputMode);
		}
		break;
	case CMoveParameterWidget::kParameterWidgetEvent:
	case CMoveParameterWidget::kParameterWidgetBool:
		{
			m_pBank->AddCombo("Pad control", &m_paramWidgetControlElement, 7, &m_controlElementNames[0]);
			m_pBank->AddToggle("Send every frame", &m_paramWidgetUpdateEveryFrame);
			m_pBank->AddToggle("Output mode", &m_paramWidgetOutputMode);
		}
		break;
	case CMoveParameterWidget::kParameterWidgetClip:
	case CMoveParameterWidget::kParameterWidgetExpression:
	case CMoveParameterWidget::kParameterWidgetFilter:
		{
			m_pBank->AddToggle("Send every frame", &m_paramWidgetUpdateEveryFrame);
		}
		break;
	
	case CMoveParameterWidgetFlag::kParameterWidgetRequest:
	case CMoveParameterWidgetFlag::kParameterWidgetFlag:
		{
			m_pBank->AddCombo("Pad control", &m_paramWidgetControlElement, 7, &m_controlElementNames[0]);
		}
		break;
	default:
		{
			// do nothing
		}
		break;
	}

	m_pBank->UnSetCurrentGroup(*m_widgetOptionsGroup);

	// Widgets rebuilt
	m_bNeedsParamWidgetOptionsRebuild = false;
}

#if __DEV

const int CAnimViewer::ms_NetworkPreviewLiveEdit_NetworkLength = 256;
char      CAnimViewer::ms_NetworkPreviewLiveEdit_Network[CAnimViewer::ms_NetworkPreviewLiveEdit_NetworkLength];
const int CAnimViewer::ms_NetworkPreviewLiveEdit_NetworkClipSetVariableLength = 256;
char      CAnimViewer::ms_NetworkPreviewLiveEdit_NetworkClipSetVariable[CAnimViewer::ms_NetworkPreviewLiveEdit_NetworkClipSetVariableLength];
const int CAnimViewer::ms_NetworkPreviewLiveEdit_NetworkClipSetLength = 256;
char      CAnimViewer::ms_NetworkPreviewLiveEdit_NetworkClipSet[CAnimViewer::ms_NetworkPreviewLiveEdit_NetworkClipSetLength];
const int CAnimViewer::ms_NetworkPreviewLiveEdit_ParamLength = 256;
char      CAnimViewer::ms_NetworkPreviewLiveEdit_Param[CAnimViewer::ms_NetworkPreviewLiveEdit_ParamLength];
float     CAnimViewer::ms_NetworkPreviewLiveEdit_Real = 0.0f;
bool      CAnimViewer::ms_NetworkPreviewLiveEdit_Bool = false;
bool      CAnimViewer::ms_NetworkPreviewLiveEdit_Flag = false;
const int CAnimViewer::ms_NetworkPreviewLiveEdit_RequestLength = 256;
char      CAnimViewer::ms_NetworkPreviewLiveEdit_Request[CAnimViewer::ms_NetworkPreviewLiveEdit_RequestLength];
bool      CAnimViewer::ms_NetworkPreviewLiveEdit_Event = false;
const int CAnimViewer::ms_NetworkPreviewLiveEdit_ClipFilenameLength = 256;
char      CAnimViewer::ms_NetworkPreviewLiveEdit_ClipFilename[CAnimViewer::ms_NetworkPreviewLiveEdit_ClipFilenameLength];
const int CAnimViewer::ms_NetworkPreviewLiveEdit_ClipSetVariableLength = 256;
char      CAnimViewer::ms_NetworkPreviewLiveEdit_ClipSetVariable[CAnimViewer::ms_NetworkPreviewLiveEdit_ClipSetVariableLength];
const int CAnimViewer::ms_NetworkPreviewLiveEdit_ClipSetLength = 256;
char      CAnimViewer::ms_NetworkPreviewLiveEdit_ClipSet[CAnimViewer::ms_NetworkPreviewLiveEdit_ClipSetLength];
const int CAnimViewer::ms_NetworkPreviewLiveEdit_ClipDictionaryLength = 256;
char      CAnimViewer::ms_NetworkPreviewLiveEdit_ClipDictionary[CAnimViewer::ms_NetworkPreviewLiveEdit_ClipDictionaryLength];
const int CAnimViewer::ms_NetworkPreviewLiveEdit_ClipLength = 256;
char      CAnimViewer::ms_NetworkPreviewLiveEdit_Clip[CAnimViewer::ms_NetworkPreviewLiveEdit_ClipLength];
const int CAnimViewer::ms_NetworkPreviewLiveEdit_ExpressionFilenameLength = 256;
char      CAnimViewer::ms_NetworkPreviewLiveEdit_ExpressionFilename[CAnimViewer::ms_NetworkPreviewLiveEdit_ExpressionFilenameLength];
const int CAnimViewer::ms_NetworkPreviewLiveEdit_ExpressionDictionaryLength = 256;
char      CAnimViewer::ms_NetworkPreviewLiveEdit_ExpressionDictionary[CAnimViewer::ms_NetworkPreviewLiveEdit_ExpressionDictionaryLength];
const int CAnimViewer::ms_NetworkPreviewLiveEdit_ExpressionLength = 256;
char      CAnimViewer::ms_NetworkPreviewLiveEdit_Expression[CAnimViewer::ms_NetworkPreviewLiveEdit_ExpressionLength];
atMap< u32, const crExpressions * > CAnimViewer::ms_NetworkPreviewLiveEdit_LoadedExpressionMap;
const int CAnimViewer::ms_NetworkPreviewLiveEdit_FilterFilenameLength = 256;
char      CAnimViewer::ms_NetworkPreviewLiveEdit_FilterFilename[CAnimViewer::ms_NetworkPreviewLiveEdit_FilterFilenameLength];
const int CAnimViewer::ms_NetworkPreviewLiveEdit_FilterDictionaryLength = 256;
char      CAnimViewer::ms_NetworkPreviewLiveEdit_FilterDictionary[CAnimViewer::ms_NetworkPreviewLiveEdit_FilterDictionaryLength];
const int CAnimViewer::ms_NetworkPreviewLiveEdit_FilterLength = 256;
char      CAnimViewer::ms_NetworkPreviewLiveEdit_Filter[CAnimViewer::ms_NetworkPreviewLiveEdit_FilterLength];
atMap< u32, crFrameFilter * > CAnimViewer::ms_NetworkPreviewLiveEdit_LoadedFilterMap;

void CAnimViewer::AddNetworkPreviewLiveEditWidgets(bkBank *pLiveEditBank)
{
	pLiveEditBank->PushGroup("MoVE Network Preview");

	pLiveEditBank->AddText("Network", ms_NetworkPreviewLiveEdit_Network, ms_NetworkPreviewLiveEdit_NetworkLength);
	pLiveEditBank->AddButton("Load network", NetworkPreviewLiveEdit_LoadNetwork);
	pLiveEditBank->AddText("Network clip set variable", ms_NetworkPreviewLiveEdit_NetworkClipSetVariable, ms_NetworkPreviewLiveEdit_NetworkClipSetVariableLength);
	pLiveEditBank->AddText("Network clip set", ms_NetworkPreviewLiveEdit_NetworkClipSet, ms_NetworkPreviewLiveEdit_NetworkClipSetLength);
	pLiveEditBank->AddButton("Set network clip set variable", NetworkPreviewLiveEdit_SetNetworkClipSetVariable);
	pLiveEditBank->AddText("Param", ms_NetworkPreviewLiveEdit_Param, ms_NetworkPreviewLiveEdit_ParamLength);
	pLiveEditBank->AddSlider("Real", &ms_NetworkPreviewLiveEdit_Real, bkSlider::FLOAT_MIN_VALUE, bkSlider::FLOAT_MAX_VALUE, 1.0f);
	pLiveEditBank->AddButton("Send real", NetworkPreviewLiveEdit_SendReal);
	pLiveEditBank->AddToggle("Bool", &ms_NetworkPreviewLiveEdit_Bool);
	pLiveEditBank->AddButton("Send bool", NetworkPreviewLiveEdit_SendBool);
	pLiveEditBank->AddToggle("Flag", &ms_NetworkPreviewLiveEdit_Flag);
	pLiveEditBank->AddButton("Send flag", NetworkPreviewLiveEdit_SendFlag);
	pLiveEditBank->AddText("Request", ms_NetworkPreviewLiveEdit_Request, ms_NetworkPreviewLiveEdit_RequestLength);
	pLiveEditBank->AddButton("Send request", NetworkPreviewLiveEdit_SendRequest);
	pLiveEditBank->AddToggle("Event", &ms_NetworkPreviewLiveEdit_Event);
	pLiveEditBank->AddButton("Send event", NetworkPreviewLiveEdit_SendEvent);
	pLiveEditBank->AddText("Clip filename", ms_NetworkPreviewLiveEdit_ClipFilename, ms_NetworkPreviewLiveEdit_ClipFilenameLength);
	pLiveEditBank->AddText("Clip set variable", ms_NetworkPreviewLiveEdit_ClipSetVariable, ms_NetworkPreviewLiveEdit_ClipSetVariableLength);
	pLiveEditBank->AddText("Clip set", ms_NetworkPreviewLiveEdit_ClipSet, ms_NetworkPreviewLiveEdit_ClipSetLength);
	pLiveEditBank->AddText("Clip dictionary", ms_NetworkPreviewLiveEdit_ClipDictionary, ms_NetworkPreviewLiveEdit_ClipDictionaryLength);
	pLiveEditBank->AddText("Clip", ms_NetworkPreviewLiveEdit_Clip, ms_NetworkPreviewLiveEdit_ClipLength);
	pLiveEditBank->AddButton("Send clip", NetworkPreviewLiveEdit_SendClip);
	pLiveEditBank->AddText("Expression filename", ms_NetworkPreviewLiveEdit_ExpressionFilename, ms_NetworkPreviewLiveEdit_ExpressionFilenameLength);
	pLiveEditBank->AddText("Expression dictionary", ms_NetworkPreviewLiveEdit_ExpressionDictionary, ms_NetworkPreviewLiveEdit_ExpressionDictionaryLength);
	pLiveEditBank->AddText("Expression", ms_NetworkPreviewLiveEdit_Expression, ms_NetworkPreviewLiveEdit_ExpressionLength);
	pLiveEditBank->AddButton("Send expression", NetworkPreviewLiveEdit_SendExpression);
	pLiveEditBank->AddText("Filter filename", ms_NetworkPreviewLiveEdit_FilterFilename, ms_NetworkPreviewLiveEdit_FilterFilenameLength);
	pLiveEditBank->AddText("Filter dictionary", ms_NetworkPreviewLiveEdit_FilterDictionary, ms_NetworkPreviewLiveEdit_FilterDictionaryLength);
	pLiveEditBank->AddText("Filter", ms_NetworkPreviewLiveEdit_Filter, ms_NetworkPreviewLiveEdit_FilterLength);
	pLiveEditBank->AddButton("Send filter", NetworkPreviewLiveEdit_SendFilter);

	pLiveEditBank->PopGroup();
}

void CAnimViewer::RemoveNetworkPreviewLiveEditWidgets(bkBank * /*pLiveEditBank*/)
{
	for(atMap< u32, const crExpressions * >::Iterator It = ms_NetworkPreviewLiveEdit_LoadedExpressionMap.CreateIterator(); !It.AtEnd(); It.Next())
	{
		const crExpressions *pExpressions = (*It);
		if(pExpressions)
		{
			pExpressions->Release();

			delete pExpressions; pExpressions = NULL;
		}
	}

	for(atMap< u32, crFrameFilter * >::Iterator It = ms_NetworkPreviewLiveEdit_LoadedFilterMap.CreateIterator(); !It.AtEnd(); It.Next())
	{
		crFrameFilter *pFilter = (*It);
		if(pFilter)
		{
			pFilter->Release();

			delete pFilter; pFilter = NULL;
		}
	}
}

void CAnimViewer::NetworkPreviewLiveEdit_LoadNetwork()
{
	if(Verifyf(strlen(ms_NetworkPreviewLiveEdit_Network) > 0, "Network filename is empty!"))
	{
		atArray<atString> networkSplit; atString(ms_NetworkPreviewLiveEdit_Network).Split(networkSplit, '.');
		if(Verifyf(networkSplit.GetCount() > 1, "Network filename does not have an extension!"))
		{
			if(Verifyf(ASSET.Exists(networkSplit[0], networkSplit[1]), "Network file does not exist!"))
			{
				fwAnimDirector::SetForceLoadAnims(true);

				LoadNetworkFromFile(ms_NetworkPreviewLiveEdit_Network);
			}
		}
	}
}

void CAnimViewer::NetworkPreviewLiveEdit_SetNetworkClipSetVariable()
{
	if(Verifyf(strlen(ms_NetworkPreviewLiveEdit_NetworkClipSetVariable) > 0, "Network clip set variable is empty!") && Verifyf(strlen(ms_NetworkPreviewLiveEdit_NetworkClipSet) > 0, "Network clip set is empty!"))
	{
		fwMvClipSetVarId clipSetVarId(ms_NetworkPreviewLiveEdit_NetworkClipSetVariable);
		fwMvClipSetId clipSetId(ms_NetworkPreviewLiveEdit_NetworkClipSet);

		CTaskPreviewMoveNetwork *pTask = FindMovePreviewTask();
		if(pTask)
		{
			fwMoveNetworkPlayer *pMoveNetworkPlayer = pTask->GetNetworkPlayer();
			if(Verifyf(pMoveNetworkPlayer, "Network player does not exist!"))
			{
				pMoveNetworkPlayer->SetClipSet(clipSetId, clipSetVarId);
			}
		}
		else
		{
			if(Verifyf(m_pDynamicEntity && m_pDynamicEntity->GetIsTypePed(), ""))
			{
				CMovePed &movePed = static_cast< CPed * >(m_pDynamicEntity.Get())->GetMovePed();

				movePed.SetClipSet(clipSetId, clipSetVarId);
			}
		}
	}
}

void CAnimViewer::NetworkPreviewLiveEdit_SendReal()
{
	if(Verifyf(strlen(ms_NetworkPreviewLiveEdit_Param) > 0, "Param expressionFilename is empty!"))
	{
		u32 param = atStringHash(ms_NetworkPreviewLiveEdit_Param);

		CTaskPreviewMoveNetwork *pTask = FindMovePreviewTask();
		if(pTask)
		{
			fwMoveNetworkPlayer *pMoveNetworkPlayer = pTask->GetNetworkPlayer();
			if(Verifyf(pMoveNetworkPlayer, "Network player does not exist!"))
			{
				pMoveNetworkPlayer->SetFloat(fwMvFloatId(param), ms_NetworkPreviewLiveEdit_Real);
			}
		}
		else
		{
			if(Verifyf(m_pDynamicEntity && m_pDynamicEntity->GetIsTypePed(), ""))
			{
				CMovePed &movePed = static_cast< CPed * >(m_pDynamicEntity.Get())->GetMovePed();
				
				movePed.SetFloat(fwMvFloatId(param), ms_NetworkPreviewLiveEdit_Real);
			}
		}
	}
}

void CAnimViewer::NetworkPreviewLiveEdit_SendBool()
{
	if(Verifyf(strlen(ms_NetworkPreviewLiveEdit_Param) > 0, "Param expressionFilename is empty!"))
	{
		u32 param = atStringHash(ms_NetworkPreviewLiveEdit_Param);

		CTaskPreviewMoveNetwork *pTask = FindMovePreviewTask();
		if(pTask)
		{
			fwMoveNetworkPlayer *pMoveNetworkPlayer = pTask->GetNetworkPlayer();
			if(Verifyf(pMoveNetworkPlayer, "Network player does not exist!"))
			{
				pMoveNetworkPlayer->SetBoolean(fwMvBooleanId(param), ms_NetworkPreviewLiveEdit_Bool);
			}
		}
		else
		{
			if(Verifyf(m_pDynamicEntity && m_pDynamicEntity->GetIsTypePed(), ""))
			{
				CMovePed &movePed = static_cast< CPed * >(m_pDynamicEntity.Get())->GetMovePed();

				movePed.SetBoolean(fwMvBooleanId(param), ms_NetworkPreviewLiveEdit_Bool);
			}
		}
	}
}

void CAnimViewer::NetworkPreviewLiveEdit_SendFlag()
{
	if(Verifyf(strlen(ms_NetworkPreviewLiveEdit_Param) > 0, "Param expressionFilename is empty!"))
	{
		u32 param = atStringHash(ms_NetworkPreviewLiveEdit_Param);

		CTaskPreviewMoveNetwork *pTask = FindMovePreviewTask();
		if(pTask)
		{
			fwMoveNetworkPlayer *pMoveNetworkPlayer = pTask->GetNetworkPlayer();
			if(Verifyf(pMoveNetworkPlayer, "Network player does not exist!"))
			{
				pMoveNetworkPlayer->SetFlag(fwMvFlagId(param), ms_NetworkPreviewLiveEdit_Flag);
			}
		}
		else
		{
			if(Verifyf(m_pDynamicEntity && m_pDynamicEntity->GetIsTypePed(), ""))
			{
				CMovePed &movePed = static_cast< CPed * >(m_pDynamicEntity.Get())->GetMovePed();

				movePed.SetFlag(fwMvFlagId(param), ms_NetworkPreviewLiveEdit_Flag);
			}
		}
	}
}

void CAnimViewer::NetworkPreviewLiveEdit_SendRequest()
{
	if(Verifyf(strlen(ms_NetworkPreviewLiveEdit_Param) > 0, "Param expressionFilename is empty!"))
	{
		u32 param = atStringHash(ms_NetworkPreviewLiveEdit_Param);

		CTaskPreviewMoveNetwork *pTask = FindMovePreviewTask();
		if(pTask)
		{
			fwMoveNetworkPlayer *pMoveNetworkPlayer = pTask->GetNetworkPlayer();
			if(Verifyf(pMoveNetworkPlayer, "Network player does not exist!"))
			{
				pMoveNetworkPlayer->BroadcastRequest(fwMvRequestId(param));
			}
		}
		else
		{
			if(Verifyf(m_pDynamicEntity && m_pDynamicEntity->GetIsTypePed(), ""))
			{
				CMovePed &movePed = static_cast< CPed * >(m_pDynamicEntity.Get())->GetMovePed();

				movePed.BroadcastRequest(fwMvRequestId(param));
			}
		}
	}
}

void CAnimViewer::NetworkPreviewLiveEdit_SendEvent()
{
	if(Verifyf(strlen(ms_NetworkPreviewLiveEdit_Param) > 0, "Param expressionFilename is empty!"))
	{
		u32 param = atStringHash(ms_NetworkPreviewLiveEdit_Param);

		CTaskPreviewMoveNetwork *pTask = FindMovePreviewTask();
		if(pTask)
		{
			fwMoveNetworkPlayer *pMoveNetworkPlayer = pTask->GetNetworkPlayer();
			if(Verifyf(pMoveNetworkPlayer, "Network player does not exist!"))
			{
				pMoveNetworkPlayer->SetBoolean(fwMvBooleanId(param), ms_NetworkPreviewLiveEdit_Event);
			}
		}
		else
		{
			if(Verifyf(m_pDynamicEntity && m_pDynamicEntity->GetIsTypePed(), ""))
			{
				CMovePed &movePed = static_cast< CPed * >(m_pDynamicEntity.Get())->GetMovePed();

				movePed.SetBoolean(fwMvBooleanId(param), ms_NetworkPreviewLiveEdit_Event);
			}
		}
	}
}

void CAnimViewer::NetworkPreviewLiveEdit_SendClip()
{
	if(Verifyf(strlen(ms_NetworkPreviewLiveEdit_Param) > 0, "Param expressionFilename is empty!"))
	{
		u32 param = atStringHash(ms_NetworkPreviewLiveEdit_Param);

		mvMotionWeb *pNetwork = NULL;
		CTaskPreviewMoveNetwork *pTask = FindMovePreviewTask();
		if(pTask)
		{
			fwMoveNetworkPlayer *pMoveNetworkPlayer = pTask->GetNetworkPlayer();
			if(Verifyf(pMoveNetworkPlayer, "Network player does not exist!"))
			{
				pNetwork = pMoveNetworkPlayer->GetNetwork();
			}
		}
		else
		{
			if(Verifyf(m_pDynamicEntity && m_pDynamicEntity->GetIsTypePed(), ""))
			{
				CMovePed &movePed = static_cast< CPed * >(m_pDynamicEntity.Get())->GetMovePed();

				pNetwork = movePed.GetNetwork();
			}
		}

		if(Verifyf(pNetwork, ""))
		{
			const crClip *pClip = NULL;

			if(strlen(ms_NetworkPreviewLiveEdit_ClipFilename) > 0)
			{
				atHashString filename(ms_NetworkPreviewLiveEdit_ClipFilename);

				pClip = fwAnimDirector::RetrieveClip(mvNodeClipDef::kClipContextLocalFile, filename.GetHash(), 0, pNetwork);
			}
			else if(strlen(ms_NetworkPreviewLiveEdit_ClipSetVariable) > 0 && strlen(ms_NetworkPreviewLiveEdit_Clip) > 0)
			{
				atHashString clipSetVariable(ms_NetworkPreviewLiveEdit_ClipSetVariable);
				atHashString clip(ms_NetworkPreviewLiveEdit_Clip);

				pClip = fwAnimDirector::RetrieveClip(mvNodeClipDef::kClipContextVariableClipSet, clipSetVariable.GetHash(), clip.GetHash(), pNetwork);
			}
			else if(strlen(ms_NetworkPreviewLiveEdit_ClipSet) > 0 && strlen(ms_NetworkPreviewLiveEdit_Clip) > 0)
			{
				atHashString clipSet(ms_NetworkPreviewLiveEdit_ClipSet);
				atHashString clip(ms_NetworkPreviewLiveEdit_Clip);

				pClip = fwAnimDirector::RetrieveClip(mvNodeClipDef::kClipContextAbsoluteClipSet, clipSet.GetHash(), clip.GetHash(), pNetwork);
			}
			else if(strlen(ms_NetworkPreviewLiveEdit_ClipDictionary) > 0 && strlen(ms_NetworkPreviewLiveEdit_Clip) > 0)
			{
				atHashString clipDictionary(ms_NetworkPreviewLiveEdit_ClipDictionary);
				atHashString clip(ms_NetworkPreviewLiveEdit_Clip);

				pClip = fwAnimDirector::RetrieveClip(mvNodeClipDef::kClipContextClipDictionary, clipDictionary.GetHash(), clip.GetHash(), pNetwork);
			}

			if(Verifyf(pClip, "Could not find clip!"))
			{
				CTaskPreviewMoveNetwork *pTask = FindMovePreviewTask();
				if(pTask)
				{
					fwMoveNetworkPlayer *pMoveNetworkPlayer = pTask->GetNetworkPlayer();
					if(Verifyf(pMoveNetworkPlayer, "Network player does not exist!"))
					{
						pMoveNetworkPlayer->SetClip(fwMvClipId(param), pClip);
					}
				}
				else
				{
					if(Verifyf(m_pDynamicEntity && m_pDynamicEntity->GetIsTypePed(), ""))
					{
						CMovePed &movePed = static_cast< CPed * >(m_pDynamicEntity.Get())->GetMovePed();

						movePed.SetClip(fwMvClipId(param), pClip);
					}
				}
			}
		}
	}
}

void CAnimViewer::NetworkPreviewLiveEdit_SendExpression()
{
	if(Verifyf(strlen(ms_NetworkPreviewLiveEdit_Param) > 0, "Param expressionFilename is empty!"))
	{
		u32 param = atStringHash(ms_NetworkPreviewLiveEdit_Param);

		mvMotionWeb *pNetwork = NULL;
		CTaskPreviewMoveNetwork *pTask = FindMovePreviewTask();
		if(pTask)
		{
			fwMoveNetworkPlayer *pMoveNetworkPlayer = pTask->GetNetworkPlayer();
			if(Verifyf(pMoveNetworkPlayer, "Network player does not exist!"))
			{
				pNetwork = pMoveNetworkPlayer->GetNetwork();
			}
		}
		else
		{
			if(Verifyf(m_pDynamicEntity && m_pDynamicEntity->GetIsTypePed(), ""))
			{
				CMovePed &movePed = static_cast< CPed * >(m_pDynamicEntity.Get())->GetMovePed();

				pNetwork = movePed.GetNetwork();
			}
		}

		if(Verifyf(pNetwork, ""))
		{
			const crExpressions *pExpression = NULL;

			if(strlen(ms_NetworkPreviewLiveEdit_ExpressionFilename) > 0)
			{
				u32 expressionFilenameHash = atStringHash(ms_NetworkPreviewLiveEdit_ExpressionFilename);
				const crExpressions **ppExpression = ms_NetworkPreviewLiveEdit_LoadedExpressionMap.Access(expressionFilenameHash);
				if(ppExpression)
				{
					pExpression = *ppExpression;
				}
				else
				{
					atArray<atString> expressionFilenameSplit; atString(ms_NetworkPreviewLiveEdit_ExpressionFilename).Split(expressionFilenameSplit, '.');
					if(Verifyf(expressionFilenameSplit.GetCount() > 1, "Expression filename does not have an extension!"))
					{
						if(Verifyf(ASSET.Exists(expressionFilenameSplit[0], expressionFilenameSplit[1]), "Expression file does not exist!"))
						{
							pExpression = crExpressions::AllocateAndLoad(ms_NetworkPreviewLiveEdit_ExpressionFilename, true);
							if(Verifyf(pExpression, "Could not allocate and load expression!"))
							{
								pExpression->AddRef();

								ms_NetworkPreviewLiveEdit_LoadedExpressionMap.Insert(expressionFilenameHash, pExpression);
							}
						}
					}
				}
			}
			else if(strlen(ms_NetworkPreviewLiveEdit_ExpressionDictionary) > 0 && strlen(ms_NetworkPreviewLiveEdit_Expression) > 0)
			{
				atHashString expressionDictionary(ms_NetworkPreviewLiveEdit_ExpressionDictionary);
				atHashString expression(ms_NetworkPreviewLiveEdit_Expression);

				pExpression = fwAnimDirector::RetrieveExpression(expressionDictionary.GetHash(), expression.GetHash(), pNetwork);
			}

			if(Verifyf(pExpression, "Could not find expression!"))
			{
				CTaskPreviewMoveNetwork *pTask = FindMovePreviewTask();
				if(pTask)
				{
					fwMoveNetworkPlayer *pMoveNetworkPlayer = pTask->GetNetworkPlayer();
					if(Verifyf(pMoveNetworkPlayer, "Network player does not exist!"))
					{
						pMoveNetworkPlayer->SetExpressions(fwMvExpressionsId(param), pExpression);
					}
				}
				else
				{
					if(Verifyf(m_pDynamicEntity && m_pDynamicEntity->GetIsTypePed(), ""))
					{
						CMovePed &movePed = static_cast< CPed * >(m_pDynamicEntity.Get())->GetMovePed();

						movePed.SetExpressions(fwMvExpressionsId(param), pExpression);
					}
				}
			}
		}
	}
}

void CAnimViewer::NetworkPreviewLiveEdit_SendFilter()
{
	if(Verifyf(strlen(ms_NetworkPreviewLiveEdit_Param) > 0, "Param name is empty!"))
	{
		u32 param = atStringHash(ms_NetworkPreviewLiveEdit_Param);

		mvMotionWeb *pNetwork = NULL;
		CTaskPreviewMoveNetwork *pTask = FindMovePreviewTask();
		if(pTask)
		{
			fwMoveNetworkPlayer *pMoveNetworkPlayer = pTask->GetNetworkPlayer();
			if(Verifyf(pMoveNetworkPlayer, "Network player does not exist!"))
			{
				pNetwork = pMoveNetworkPlayer->GetNetwork();
			}
		}
		else
		{
			if(Verifyf(m_pDynamicEntity && m_pDynamicEntity->GetIsTypePed(), ""))
			{
				CMovePed &movePed = static_cast< CPed * >(m_pDynamicEntity.Get())->GetMovePed();

				pNetwork = movePed.GetNetwork();
			}
		}

		if(Verifyf(pNetwork, ""))
		{
			crFrameFilter *pFilter = NULL;

			if(strlen(ms_NetworkPreviewLiveEdit_FilterFilename) > 0)
			{
				u32 filterFilenameHash = atStringHash(ms_NetworkPreviewLiveEdit_FilterFilename);
				crFrameFilter **ppFilter = ms_NetworkPreviewLiveEdit_LoadedFilterMap.Access(filterFilenameHash);
				if(ppFilter)
				{
					pFilter = *ppFilter;
				}
				else
				{
					atArray<atString> filterFilenameSplit; atString(ms_NetworkPreviewLiveEdit_FilterFilename).Split(filterFilenameSplit, '.');
					if(Verifyf(filterFilenameSplit.GetCount() > 1, "Filter filename does not have an extension!"))
					{
						if(Verifyf(ASSET.Exists(filterFilenameSplit[0], filterFilenameSplit[1]), "Filter file does not exist!"))
						{
							pFilter = crFrameFilter::AllocateAndLoad(ms_NetworkPreviewLiveEdit_FilterFilename);
							if(Verifyf(pFilter, "Could not allocate and load filter!"))
							{
								pFilter->AddRef();

								ms_NetworkPreviewLiveEdit_LoadedFilterMap.Insert(filterFilenameHash, pFilter);
							}
						}
					}
				}
			}
			else if(strlen(ms_NetworkPreviewLiveEdit_FilterDictionary) > 0 && strlen(ms_NetworkPreviewLiveEdit_Filter) > 0)
			{
				atHashString filterDictionary(ms_NetworkPreviewLiveEdit_FilterDictionary);
				atHashString filter(ms_NetworkPreviewLiveEdit_Filter);

				pFilter = fwAnimDirector::RetrieveFilter(filterDictionary.GetHash(), filter.GetHash(), pNetwork);
			}

			if(Verifyf(pFilter, "Could not find filter!"))
			{
				CTaskPreviewMoveNetwork *pTask = FindMovePreviewTask();
				if(pTask)
				{
					fwMoveNetworkPlayer *pMoveNetworkPlayer = pTask->GetNetworkPlayer();
					if(Verifyf(pMoveNetworkPlayer, "Network player does not exist!"))
					{
						pMoveNetworkPlayer->SetFilter(fwMvFilterId(param), pFilter);
					}
				}
				else
				{
					if(Verifyf(m_pDynamicEntity && m_pDynamicEntity->GetIsTypePed(), ""))
					{
						CMovePed &movePed = static_cast< CPed * >(m_pDynamicEntity.Get())->GetMovePed();

						movePed.SetFilter(fwMvFilterId(param), pFilter);
					}
				}
			}
		}
	}
}

#endif // __DEV

void CAnimViewer::UpdateMoveClipSet()
{
	if (m_pDynamicEntity)
	{
		if (m_pDynamicEntity->GetIsTypePed() || m_pDynamicEntity->GetIsTypeObject() || m_pDynamicEntity->GetIsTypeVehicle())
		{
			fwMvClipSetVarId clipSetVarId(ms_clipSetVariableName);
			if(clipSetVarId == CLIP_SET_VAR_ID_INVALID)
			{
				clipSetVarId = CLIP_SET_VAR_ID_DEFAULT;
			}

			fwMvClipSetId clipSetId(fwClipSetManager::GetClipSetNames()[m_moveNetworkClipSet]);

			// get the anim preview task
			CTaskPreviewMoveNetwork* pTask = FindMovePreviewTask();

			if (pTask)
			{
				pTask->ApplyClipSet(clipSetId, clipSetVarId);

				ms_updateClipSetVariables = true;
			}
		}
	}
}

char CAnimViewer::ms_clipSetVariableName[64];
bool CAnimViewer::ms_updateClipSetVariables = true;

void CAnimViewer::CreateClipSetVariable()
{
	fwMvClipSetId clipSetId(fwClipSetManager::GetClipSetNames()[m_moveNetworkClipSet]);
	if(!fwClipSetManager::GetClipSet(clipSetId))
	{
		clipSetId = CLIP_SET_ID_INVALID;
	}

	fwMvClipSetVarId clipSetVarId(ms_clipSetVariableName);
	if(clipSetVarId == CLIP_SET_VAR_ID_INVALID)
	{
		clipSetVarId = CLIP_SET_VAR_ID_DEFAULT;
	}

	CTaskPreviewMoveNetwork *taskPreviewMoveNetwork = FindMovePreviewTask();
	if(taskPreviewMoveNetwork)
	{
		taskPreviewMoveNetwork->ApplyClipSet(clipSetId, clipSetVarId);

		ms_updateClipSetVariables = true;
	}
}

void CAnimViewer::DeleteClipSetVariable()
{
	fwMvClipSetVarId clipSetVarId(ms_clipSetVariableName);
	if(clipSetVarId == CLIP_SET_VAR_ID_INVALID)
	{
		clipSetVarId = CLIP_SET_VAR_ID_DEFAULT;
	}

	CTaskPreviewMoveNetwork *taskPreviewMoveNetwork = FindMovePreviewTask();
	if(taskPreviewMoveNetwork)
	{
		taskPreviewMoveNetwork->ApplyClipSet(CLIP_SET_ID_INVALID, clipSetVarId);

		ms_updateClipSetVariables = true;
	}
}

void CAnimViewer::UpdateClipSetVariables()
{
	if(ms_updateClipSetVariables)
	{
		CTaskPreviewMoveNetwork *taskPreviewMoveNetwork = FindMovePreviewTask();
		if(taskPreviewMoveNetwork)
		{
			fwMoveNetworkPlayer *moveNetworkPlayer = taskPreviewMoveNetwork->GetNetworkPlayer();
			if(moveNetworkPlayer)
			{
				while (m_clipSetVariableGroup->GetChild())
				{
					m_clipSetVariableGroup->Remove(*(m_clipSetVariableGroup->GetChild()));
				}

				atMap< fwMvClipSetVarId, fwMvClipSetId >::ConstIterator entry = moveNetworkPlayer->GetClipSetIdMap().CreateIterator();
				for(entry.Start(); !entry.AtEnd(); entry.Next())
				{
					fwMvClipSetVarId clipSetVarId = entry.GetKey();
					fwMvClipSetId clipSetId = entry.GetData();

					m_clipSetVariableGroup->AddTitle("%s -> %s", clipSetVarId.GetCStr(), clipSetId.GetCStr());
				}
			}
		}

		ms_updateClipSetVariables = false;
	}
}

CTaskPreviewMoveNetwork* CAnimViewer::FindMovePreviewTask()
{	
	if (m_pDynamicEntity)
	{
		if (m_pDynamicEntity->GetIsTypePed())
		{
			CPed* pPed = static_cast<CPed*>(m_pDynamicEntity.Get());
			if (pPed)
			{
				//try searching in the primary tree
				CTaskPreviewMoveNetwork* pTask = static_cast<CTaskPreviewMoveNetwork*>(pPed->GetPedIntelligence()->FindTaskByType(CTaskTypes::TASK_PREVIEW_MOVE_NETWORK));
				
				if (pTask)
				{
					return pTask;
				}
				else
				{
					//Search for the secondary task
					CTask* pSecondary = pPed->GetPedIntelligence()->GetTaskSecondary(0);
					if (pSecondary && pSecondary->GetTaskType()==CTaskTypes::TASK_PREVIEW_MOVE_NETWORK)
					{
						return static_cast<CTaskPreviewMoveNetwork*>(pSecondary);
					}
				}
			}
		}
		else if (m_pDynamicEntity->GetIsTypeObject())
		{
			CObject* pObj = static_cast<CObject*>(m_pDynamicEntity.Get());
			if (pObj && pObj->GetObjectIntelligence())
			{
				return static_cast<CTaskPreviewMoveNetwork*>(pObj->GetObjectIntelligence()->FindTaskByType(CTaskTypes::TASK_PREVIEW_MOVE_NETWORK));
			}
		}
		else if (m_pDynamicEntity->GetIsTypeVehicle())
		{
			CVehicle* pVeh = static_cast<CVehicle*>(m_pDynamicEntity.Get());
			if (pVeh && pVeh->GetIntelligence() && pVeh->GetIntelligence()->GetTaskManager())
			{
				return static_cast<CTaskPreviewMoveNetwork*>(pVeh->GetIntelligence()->GetTaskManager()->FindTaskByTypeActive(VEHICLE_TASK_TREE_SECONDARY, CTaskTypes::TASK_PREVIEW_MOVE_NETWORK));
			}
		}
	}

	return NULL;
}

void CAnimViewer::AddParamWidget()
{
	if (m_pDynamicEntity)
	{
		// get the anim preview task
		CTaskPreviewMoveNetwork* pTask = FindMovePreviewTask();

		if (pTask)
		{
			fwMove* pMove = &m_pDynamicEntity->GetAnimDirector()->GetMove();
			fwMoveNetworkPlayer* handle = pTask->GetNetworkPlayer();

			CMoveParameterWidget* newParam = NULL;

			switch(m_newParamType)
			{
			case CMoveParameterWidget::kParameterWidgetReal:
				newParam = rage_new CMoveParameterWidgetReal( pMove, handle, &m_newParamName[0], m_paramWidgetUpdateEveryFrame, m_paramWidgetMinValue, m_paramWidgetMaxValue, m_paramWidgetOutputMode, m_paramWidgetAnalogueControlElement);
				break;
			case CMoveParameterWidget::kParameterWidgetBool:
				newParam = rage_new CMoveParameterWidgetBool( pMove, handle, &m_newParamName[0], m_paramWidgetUpdateEveryFrame, m_paramWidgetOutputMode, m_paramWidgetControlElement);
				break;
			case CMoveParameterWidget::kParameterWidgetFlag:
				newParam = rage_new CMoveParameterWidgetFlag( pMove, handle, &m_newParamName[0], m_paramWidgetControlElement);
				break;
			case CMoveParameterWidget::kParameterWidgetRequest:
				newParam = rage_new CMoveParameterWidgetRequest( pMove, handle, &m_newParamName[0], m_paramWidgetControlElement);
				break;
			case CMoveParameterWidget::kParameterWidgetEvent:
				newParam = rage_new CMoveParameterWidgetEvent( pMove, handle, &m_newParamName[0], m_paramWidgetControlElement);
				break;
			case CMoveParameterWidget::kParameterWidgetClip:
				newParam = rage_new CMoveParameterWidgetClip( pMove, handle, &m_newParamName[0], false);
				break;
			case CMoveParameterWidget::kParameterWidgetExpression:
				newParam = rage_new CMoveParameterWidgetExpression( pMove, handle, &m_newParamName[0], false);
				break;
			case CMoveParameterWidget::kParameterWidgetFilter:
				newParam = rage_new CMoveParameterWidgetFilter( pMove, handle, &m_newParamName[0], false);
				break;
			}

			m_pBank->SetCurrentGroup(*m_previewNetworkGroup);

			newParam->AddWidgets(m_pBank);

			m_pBank->UnSetCurrentGroup(*m_previewNetworkGroup);

			pTask->AddParameterHookup(newParam);
		}
	}
}

void CAnimViewer::ClearParamWidgets()
{
	if (m_pDynamicEntity)
	{
		// get the anim preview task
		CTaskPreviewMoveNetwork* pTask = FindMovePreviewTask();

		if (pTask)
		{
			pTask->ClearParamWidgets();
		}
	}
}

void CAnimViewer::SaveParamWidgets()
{
	if (m_pDynamicEntity)
	{
		// get the anim preview task
		CTaskPreviewMoveNetwork* pTask = FindMovePreviewTask();

		if (pTask)
		{
			pTask->SaveParamWidgets();
		}
	}
}

void CAnimViewer::ForceSelectState()
{
	if (m_pDynamicEntity && (m_pDynamicEntity->GetIsTypePed() || m_pDynamicEntity->GetIsTypeObject() || m_pDynamicEntity->GetIsTypeVehicle()))
	{
		CTaskPreviewMoveNetwork* task = FindMovePreviewTask();
		if (task)
		{
			fwMoveNetworkPlayer* network = task->GetNetworkPlayer();
			network->ForceState( fwMvStateId(m_stateMachine), fwMvStateId(m_forceToState));
		}
	}
}


bool DumpAnimationWRTSelectedEntity(CDynamicEntity *pDynamicEntity, const crAnimation& anim, u8 version, bool csv, bool tracks, bool blocks, bool chunks, bool channels, bool values, bool eulers, bool memory, const crSkeletonData* skelData)
{
	if(csv)
	{
		Printf("Filename,Version,Duration,Fps,InternalFrames,FramesPerChunk,MaxBlockSize,PackedMemUse,CompactMemUse,Looped,HasMover,Raw,NumTracks,NumBlocks\n");
	}

	// print basic info:
	Printf(csv?"%s,":"Filename:\t%s\n", anim.GetName());
	Printf(csv?"%d,":"Version:\t%d\n", version);
	Printf(csv?"%f,":"Duration:\t%f\n", anim.GetDuration());
	Printf(csv?"%f,":"Fps:\t\t%f\n", float(Max(1,int(anim.GetNumInternalFrames())-1))/anim.GetDuration());
	Printf(csv?"%d,":"InternalFrames:\t%d\n", anim.GetNumInternalFrames());
	Printf(csv?"%d,":"FramesPerChunk:\t%d\n", anim.GetNumInternalFramesPerChunk());
	Printf(csv?"%d,":"MaxBlockSize:\t%d\n", anim.GetMaxBlockSize());
	Printf(csv?"%d,":"MemUse:\t%d\n", anim.ComputeSize());
	Printf(csv?"%d,":"IsLooped:\t%d\n", anim.IsLooped());
	Printf(csv?"%d,":"HasMover:\t%d\n", anim.HasMoverTracks());
	Printf(csv?"%d,":"Raw:\t\t%d\n", anim.IsRaw());
	Printf(csv?"%d,":"NumTracks:\t%d\n", anim.GetNumTracks());
	Printf(csv?"%d":"NumBlocks:\t%d\n", anim.GetNumBlocks());

	// print more verbose info:
	if(tracks || values)
	{
		Printf("\n");
		if(csv)
		{
			Printf("\nTrackIdx,Track,TrackId,TrackType,TrackName");
			Printf(memory?",TrackMemUse":"");
			if(values)
			{
				Printf(",TrackValues");
			}
			Printf("\n");
		}

		for(int i=0; i<anim.GetNumTracks(); i++)
		{
			crAnimTrack* track = anim.GetTrack(i);

			char trackName[255];
			sprintf(trackName, "%s", crAnimTrack::ConvertTrackIndexToName(track->GetTrack()));
			if(skelData)
			{
				switch(track->GetTrack())
				{
				case kTrackBoneTranslation:
				case kTrackBoneRotation:
				case kTrackBoneScale:
					{
						int boneIdx = -1;
						skelData->ConvertBoneIdToIndex(track->GetId(), boneIdx);
						sprintf(&trackName[strlen(trackName)], ":%s", (boneIdx>=0)?skelData->GetBoneData(boneIdx)->GetName():"unknown");
					}
					break;
				}
			}
			Printf(csv?"%d,%d,%d,%d,%s":"Track[%d]:\ttrack %d id %d type %d %s", i, int(track->GetTrack()), int(track->GetId()), int(track->GetType()), (trackName[0]?trackName:"unknown"));

			if(memory)
			{
				Printf(csv?",%d":" (%d bytes)", int(track->ComputeSize()));
			}

			if(values)
			{
				if(!csv)
				{
					Printf("\n");
				}

				for(int j=0; j<anim.GetNumInternalFrames(); ++j)
				{
					float t = float(j);

					switch(track->GetType())
					{
					case kFormatTypeFloat:
						{	
							float f;
							track->EvaluateFloat(t, f);
							Printf(csv?",%d,%f":"Value[%d]:\t%f\n", j, f);
						}
						break;

					case kFormatTypeVector3:
						{	
							Vec3V v;
							track->EvaluateVector3(t, v);
							Printf(csv?"%d,%f,%f,%f":"Value[%d]:\t%f %f %f\n", j, v[0], v[1], v[2]);
						}
						break;

					case kFormatTypeQuaternion:
						{	
							QuatV q;
							track->EvaluateQuaternion(t, q);
							if(eulers)
							{
								Vec3V e = QuatVToEulersXYZ(q);
								e *= ScalarVFromF32(RtoD);
								Printf(csv?",%d,%f,%f,%f":"Value[%d]:\t%f %f %f\n", j, e[0], e[1], e[2]);
							}
							else
							{
								Printf(csv?",%d,%f,%f,%f,%f":"Value[%d]:\t%f %f %f %f\n", j, q[0], q[1], q[2], q[3]);
							}
						}
						break;

					default: 
						Printf(csv?"%d,?":"Value[%d]:\t?\n", j);
					}
				}
			}
			Printf("\n");
		}
	}

	Printf("\n");

	if(blocks || chunks || channels)
	{
		const u16 numBlocks = u16(anim.GetNumBlocks());
		for(u16 i=0; i<numBlocks; ++i)
		{
			Printf("\n");
			if(csv)
			{
				Printf("\nBlockIdx,BlockSize,CompactBlockSize,CompactSlopSize");
				if(chunks || channels)
				{
					Printf(",ChunkAndChannels");
				}
				Printf("\n");
			}

			const crBlockStream* block = anim.GetBlock(i);
			const crBlockStream* compactBlock = anim.GetBlock(i);

			Printf(csv?"%d,%d,%d,%d":"Block[%d]:\tblock size %d compact block size %d compact slop size %d", i, block->m_DataSize, compactBlock->m_CompactSize, compactBlock->m_SlopSize);
		}
	}

	////////// ********* NOTE -- a bunch of the Printf's below have a completely bogus format specified when 'csv' is true.
	if (pDynamicEntity)
	{
		const crSkeletonData &skelData = pDynamicEntity->GetSkeletonData();

		crFrame animFrame;
		animFrame.InitCreateBoneAndMoverDofs(skelData);
		animFrame.IdentityFromSkel(skelData);

		crFrameDataFixedDofs<2> frameDataDOFs;
		frameDataDOFs.AddDof(kTrackMoverTranslation, 0, kFormatTypeVector3);
		frameDataDOFs.AddDof(kTrackMoverRotation, 0, kFormatTypeQuaternion);
		crFrameFixedDofs<2> frameDOFs(frameDataDOFs);

		// Mover DOFs
		for(int i=0; i<anim.GetNumInternalFrames(); ++i)
		{
			float frame = float(i);
			//anim.CompositeFrameWithMover(anim.Convert30FrameToTime(frame),0.0f, animFrame);
			anim.CompositeFrame(anim.Convert30FrameToTime(frame), frameDOFs, false, false);
			int j = 0;

			Vec3V v;
			// Mover translation
			u16 boneId = 0;
			bool validDOF = frameDOFs.GetValue<Vec3V>(kTrackMoverTranslation, boneId, v);
			if (validDOF)
			{	
				Printf(csv?"%d,%d,%d,%f,%f,%f":"Frame[%d]: Index[%d]: Name[Mover]: Id[%d]: Translation:\t%f %f %f\n", i, j, boneId, v[0], v[1], v[2]);
			}
			else
			{
				Printf(csv?",%d,%d,%d":"Frame[%d]: Index[%d]: Name[Mover]: Id[%d]: Invalid DOF\n", i, j, boneId);
			}
		}

		// Mover DOFs
		for(int i=0; i<anim.GetNumInternalFrames(); ++i)
		{
			float frame = float(i);
			//anim.CompositeFrameWithMover(anim.Convert30FrameToTime(frame),0.0f, animFrame);
			anim.CompositeFrame(anim.Convert30FrameToTime(frame), frameDOFs, false, false);
			int j = 0;

			QuatV q;
			// Mover rotation
			u16 boneId = 0;
			bool validDOF = frameDOFs.GetValue<QuatV>(kTrackMoverRotation, boneId, q);
			if (validDOF)
			{
				if(eulers)
				{
					Vec3V e = QuatVToEulersXYZ(q);
					e *= ScalarVFromF32(RtoD);
					Printf(csv?",%d,%d,%d,%f,%f,%f":"Frame[%d]: Index[%d]: Name[Mover]: Id[%d]: Rotation:\t%f %f %f\n", i, j, boneId, e[0], e[1], e[2]);
				}
				else
				{
					Printf(csv?",%d,%d,%d,%f,%f,%f,%f":"Frame[%d]: Index[%d]: Name[Mover]: Id[%d]: Rotation:\t%f %f %f %f\n", i, j, boneId, q[0], q[1], q[2], q[3]);
				}						
			}
			else
			{
				Printf(csv?",%d,%d,%d":"Frame[%d]: Index[%d]: Name[Mover]: Id[%d]: Invalid DOF\n", i, j, boneId);
			}		
		}

		for(int j=0; j<skelData.GetNumBones(); ++j)
		{
			const crBoneData* pBoneData = skelData.GetBoneData(j);
			if (pBoneData)
			{
				u16 boneId = pBoneData->GetBoneId();
				const char* pName = pBoneData->GetName();

				for(int i=0; i<anim.GetNumInternalFrames(); ++i)
				{
					float frame = float(i);
					anim.CompositeFrameWithMover(anim.Convert30FrameToTime(frame),0.0f, animFrame, false, false);

					if (pBoneData->HasDofs(crBoneData::ROTATION))
					{
						QuatV q;
						bool validDOF = animFrame.GetBoneRotation(boneId, q);
						if (validDOF)
						{
							if(eulers)
							{
								Vec3V e = QuatVToEulersXYZ(q);
								e *= ScalarVFromF32(RtoD);
								Printf(csv?",%d,%d,%s,%d,%f,%f,%f":"Frame[%d]: Index[%d]: Name[%s]: Id[%d]: Rotation:\t%f %f %f\n", i, j, pName, boneId, e[0], e[1], e[2]);
							}
							else
							{
								Printf(csv?",%d,%d,%s,%d,%f,%f,%f,%f":"Frame[%d]: Index[%d]: Name[%s]: Id[%d]: Rotation:\t%f %f %f %f\n", i, j, pName, boneId, q[0], q[1], q[2], q[3]);
							}						
						}
						else
						{
							Printf(csv?",%d,%d,%s,%d":"Frame[%d]: Index[%d]: Name[%s]: Id[%d]: Invalid DOF\n", i, j, pName, boneId);
						}

					}
				}

				for(int i=0; i<anim.GetNumInternalFrames(); ++i)
				{
					float frame = float(i);
					anim.CompositeFrameWithMover(anim.Convert30FrameToTime(frame),0.0f, animFrame, false, false);

					if (pBoneData->HasDofs(crBoneData::ROTATION))
					{
						Vec3V v;
						bool validDOF = animFrame.GetBoneTranslation(boneId, v);
						if (validDOF)
						{	
							Printf(csv?"%d,%d,%s,%d,%f,%f,%f":"Frame[%d]: Index[%d]: Name[%s]: Id[%d]: Translation:\t%f %f %f\n", i, j, pName, boneId, v[0], v[1], v[2]);
						}
						else
						{
							Printf(csv?",%d,%d,%s,%d":"Frame[%d]: Index[%d]: Name[%s]: Id[%d]: Invalid DOF\n", i, j, pName, boneId);
						}
					}
				}
			}
		}
	}

	Printf("\n");

	return true;
}


void CAnimViewer::DumpClip()
{
	crClip* pClip = m_animSelector.GetSelectedClip();
	if (pClip)
	{
		crDumpOutputStdIo output;
		pClip->Dump(output);
	}
	else
	{
		Printf("CAnimViewer::DumpClip - No clip selected\n");
	}
}

void CAnimViewer::DumpAnimWRTSelectedEntity()
{
	bool tracks = true;
	bool blocks = false;
	bool chunks = false;
	bool channels = false;
	bool values = true;
	bool eulers = true;
	bool csv = false;
	bool memory = true;
	u8 version = 1;


	crClip* pClip = m_animSelector.GetSelectedClip();
	if (pClip)
	{
		switch(pClip->GetType())
		{
		case crClip::kClipTypeAnimation:
			{
			crClipAnimation* pClipAnim = static_cast<crClipAnimation*>(pClip);
			const crAnimation *pAnim =  pClipAnim->GetAnimation();					
			if (pAnim)
			{
				DumpAnimationWRTSelectedEntity(m_pDynamicEntity, *pAnim, version, csv, tracks, blocks, chunks, channels, values, eulers, memory, NULL);
			}
				break;
			}

		case crClip::kClipTypeAnimations:
			{
				crClipAnimations* pClipAnims = static_cast<crClipAnimations*>(pClip);
				unsigned int numAnims = pClipAnims->GetNumAnimations();
				for(unsigned int i=0; i < numAnims; i++)
				{
					const crAnimation *pAnim =  pClipAnims->GetAnimation(i);
					if(pAnim)
					{
						DumpAnimationWRTSelectedEntity(m_pDynamicEntity, *pAnim, version, csv, tracks, blocks, chunks, channels, values, eulers, memory, NULL);
			}
		}
				break;
			}

		default:
			Printf("CAnimViewer::DumpAnimWRTSelectedEntity - Invalid clip type\n");
		}
	}
	else
	{
		Printf("CAnimViewer::DumpAnimWRTSelectedEntity - No clip selected\n");
	}
}

bool CAnimViewer::m_bDumpPose = false;
bool CAnimViewer::m_bDumpPosePrintEulers = false;
bool CAnimViewer::m_bDumpPosePrintOnlyDOFsInClip = true;
int CAnimViewer::m_DumpPosePrintIndent = 0;
int CAnimViewer::m_DumpPoseFrameIndex = 0;
int CAnimViewer::m_DumpPoseFrameCount = 0;
fiStream *CAnimViewer::m_pDumpPoseFile = NULL;
CGenericClipHelper CAnimViewer::m_DumpPoseClipHelper;

void CAnimViewer::DumpPose()
{
	if(animVerifyf(!m_bDumpPose && !m_pDumpPoseFile, ""))
	{
		if(animVerifyf(m_pDynamicEntity && m_pDynamicEntity->GetIsTypePed(), ""))
		{
			CPed *pPed = static_cast< CPed * >(m_pDynamicEntity.Get());
			const crClip *pClip = m_animSelector.GetSelectedClip();
			if(animVerifyf(pClip, ""))
			{
				ASSET.PushFolder(RS_PROJROOT);

				atString filename(atVarString("%s@%s_DumpPose", m_animSelector.GetSelectedClipDictionary(), m_animSelector.GetSelectedClipName()));
				m_pDumpPoseFile = ASSET.Create(filename.c_str(), "xml");
				if(animVerifyf(m_pDumpPoseFile, "Could not create " RS_PROJROOT "/%s.xml!", filename.c_str()))
				{
					m_DumpPoseFrameIndex = 0;
					m_DumpPoseFrameCount = static_cast< int >(pClip->GetNum30Frames());

					m_DumpPoseClipHelper.BlendInClipByClip(pPed, pClip, INSTANT_BLEND_IN_DELTA, INSTANT_BLEND_OUT_DELTA, false, true);
					m_DumpPoseClipHelper.SetRate(0.0f);

					pPed->GetPedIntelligence()->ClearTasks();
					pPed->GetMovePed().SetTaskNetwork(m_DumpPoseClipHelper);

					// Write xml declaration
					DumpPosePrint("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");

					// Write animation element (open)
					DumpPosePrint("<animation name=\"%s\" dictionary=\"%s\" duration=\"%.3f\" framecount=\"%i\" onlyDOFsInClip=\"%s\">", m_animSelector.GetSelectedClipName(), m_animSelector.GetSelectedClipDictionary(), pClip->GetDuration(), m_DumpPoseFrameCount, m_bDumpPosePrintOnlyDOFsInClip ? "true" : "false");
					m_DumpPosePrintIndent ++;

					m_bDumpPose = true;

					animDisplayf("Dumping pose to " RS_PROJROOT "/%s.xml...", filename.c_str());
				}
			}
		}
	}
}

void CAnimViewer::DumpPosePrint(const char *szFormat, ...)
{
	va_list argptr;
	va_start(argptr, szFormat);

	char buffer[1024]; buffer[0] = '\0';

	for(int i = 0; i < m_DumpPosePrintIndent; i ++)
	{
		sprintf(&buffer[strlen(buffer)], "\t");
	}

	vsprintf(&buffer[strlen(buffer)], szFormat, argptr);

	sprintf(&buffer[strlen(buffer)], "\n");

	m_pDumpPoseFile->WriteByte(buffer, istrlen(buffer));

	va_end(argptr);
}

void CAnimViewer::BatchProcessClipsOnDisk()
{
	animDisplayf("Begin on disk process(es):");

	CClipFileIterator iter(MakeFunctorRet(CAnimViewer::ProcessClipOnDisk), m_BatchProcessInputFolder, true);
	iter.Iterate();

	animDisplayf("Clip processing complete.");
}

void CAnimViewer::BatchProcessClipsInMemory()
{
	animDisplayf("Begin in memory process(es):");
	CDebugClipDictionary::IterateClips(MakeFunctor(CAnimViewer::ProcessClipInImage));
	animDisplayf("Clip processing complete.");
}

bool CAnimViewer::ProcessClipOnDisk(rage::crClip* pClip, fiFindData UNUSED_PARAM(fileData), const char* directory)
{
	bool bClipUpdated = false;

	atString clipName(pClip->GetName());
	clipName.Lowercase();

	if (m_bVerbose)
	{
		Displayf("clip[%s] Processing...", clipName.c_str());
	}

	if (pClip->GetType() == crClip::kClipTypeAnimation)
	{
		crClipAnimation* pClipAnim = static_cast<crClipAnimation*>(pClip);

		if (pClipAnim)
		{
			crAnimation* pAnim = const_cast<crAnimation*>(pClipAnim->GetAnimation());

			if (pAnim)
			{
				bool bAnimUpdated = false;

				if (m_abClipProcessEnabled[CP_ADD_TRACK_IK_ROOT])
				{
					bAnimUpdated |= AddTrackIKRoot(pAnim, clipName);
				}

				if (m_abClipProcessEnabled[CP_ADD_TRACK_IK_HEAD])
				{
					bAnimUpdated |= AddTrackIKHead(pAnim, clipName);
				}

				if (m_abClipProcessEnabled[CP_ADD_TRACK_SPINE_WEIGHT])
				{
					bAnimUpdated |= AddTrackSpineStabilizationWeight(pAnim, clipName);
				}

				if (m_abClipProcessEnabled[CP_ADD_TRACK_ROOT_HEIGHT])
				{
					bAnimUpdated |= AddTrackRootHeight(pClip, pAnim, clipName);
				}

				if (m_abClipProcessEnabled[CP_ADD_TRACK_HIGH_HEEL])
				{
					bAnimUpdated |= AddTrackHighHeelControl(pAnim, clipName);
				}

				if (m_abClipProcessEnabled[CP_ADD_TRACK_FIRST_PERSON_CAMERA_WEIGHT])
				{
					bAnimUpdated |= AddTrackFirstPersonCameraWeight(pAnim, clipName);
				}

				if (m_abClipProcessEnabled[CP_CHECK_IK_ROOT])
				{
					CheckIkRoot(pAnim, clipName);
				}

				if (m_abClipProcessEnabled[CP_RESET_IK_ROOT])
				{
					bAnimUpdated |= ResetIkRoot(pAnim, clipName);
				}

				if (m_abClipProcessEnabled[CP_DELETE_TRACK_ARM_ROLL])
				{
					bAnimUpdated |= DeleteTrackArmRoll(pAnim, clipName);
				}

				if (m_abClipProcessEnabled[CP_CHECK_ANIM_DATA_MOVER_ONLY])
				{
					CheckAnimDataMoverOnly(pAnim, clipName);
				}

				if (m_abClipProcessEnabled[CP_CHECK_CONSTRAINT_HELPER])
				{
					CheckConstraintHelper(pAnim, clipName);
				}

				if (m_abClipProcessEnabled[CP_CONVERT_FIRST_PERSON_DATA])
				{
					bAnimUpdated |= ConvertFirstPersonData(pClip, pAnim, clipName);
				}

				if (bAnimUpdated)
				{
					char sPath[RAGE_MAX_PATH];
					ASSET.RemoveNameFromPath(sPath, sizeof(sPath), pClip->GetName());

					atString sFilename(sPath);
					sFilename.Replace(m_BatchProcessInputFolder, m_BatchProcessOutputFolder);
					sFilename += "/";
					sFilename += pAnim->GetName();

					ASSET.CreateLeadingPath(sFilename.c_str());

					if (m_bVerbose)
					{
						Displayf("clip[%s] Saving animation to %s", clipName.c_str(), sFilename.c_str());
					}
					pAnim->Save(sFilename.c_str());
				}
				else
				{
					if (m_bVerbose)
					{
						Displayf("clip[%s] Animation skipped", clipName.c_str());
					}
				}
			}
		}
	}

	if (m_abClipProcessEnabled[CP_ADD_TAGS_LOOK_IK])
	{
		bClipUpdated |= AddTagsLookIK(pClip, clipName);
	}

	if (m_abClipProcessEnabled[CP_CHECK_CLIP_ANIM_DURATION])
	{
		CheckClipAnimDuration(pClip, clipName, directory);
	}

	if (m_abClipProcessEnabled[CP_CHECK_TAGS_RANGE])
	{
		CheckTagsRange(pClip, clipName, directory);
	}

	if (m_abClipProcessEnabled[CP_UPDATE_AUDIO_RELEVANT_WEIGHT_THRESHOLD])
	{
		bClipUpdated |= UpdateAudioRelevantWeightThreshold(pClip, clipName);
	}

	if (m_abClipProcessEnabled[CP_CHECK_FOR_AUDIO_TAGS])
	{
		CheckForAudioTags(pClip, clipName);
	}

	return bClipUpdated; // return true if you want to save the clip back to disk.
}

void CAnimViewer::ProcessClipInImage( rage::crClip * pClip, rage::crClipDictionary* UNUSED_PARAM(pDict), strLocalIndex dictIndex)
{	
	//////////////////////////////////////////////////////////////////////////
	// Add your clip processing here
	//////////////////////////////////////////////////////////////////////////

	atString clipName(pClip->GetName());
	clipName.Replace("pack:/", "");

	/*
	TUNE_GROUP_BOOL(CLIP_VIEWER, bOutputClipsNullAnim, false);
	TUNE_GROUP_BOOL(CLIP_VIEWER, bOutputClipsRaw, false);
	TUNE_GROUP_BOOL(CLIP_VIEWER, bOutputClipsInvalidMaxBlockSize, true);
	TUNE_GROUP_BOOL(CLIP_VIEWER, bOutputClipsNotCompactOnPS3, false);
	if (pClip)
	{
		//Displayf("%s / %s", fwAnimManager::GetName(dictIndex), pClip->GetName());
		if (pClip->GetType()==crClip::kClipTypeAnimation)
		{
			crClipAnimation* pClipAnim = static_cast<crClipAnimation*>(pClip);
			if(pClipAnim)
			{
				const crAnimation *pAnim =  pClipAnim->GetAnimation();					
				if (pAnim)
				{		
					if (bOutputClipsRaw && pAnim->IsRaw())
					{
						Displayf("Anim is raw - %s, %s\n", fwAnimManager::GetName(dictIndex), clipName.c_str());
					}

					if (bOutputClipsInvalidMaxBlockSize && pAnim->GetMaxBlockSize() > 64*1024)
					{
						Displayf("Anim MaxBlockSize invalid - %s, %s, %d, %d\n", fwAnimManager::GetName(dictIndex), clipName.c_str(), pAnim->GetMaxBlockSize(), pAnim->IsRaw());
					}

#if __PS3
					if (bOutputClipsNotCompactOnPS3 && !pAnim->IsCompact())
					{
						Displayf("Anim (on PS3) is not compact - %s, %s\n", fwAnimManager::GetName(dictIndex), clipName.c_str());
					}
#endif //__PS3
					}
				else if (bOutputClipsNullAnim)
				{					
					Displayf("Anim is null - %s, %s\n", fwAnimManager::GetName(dictIndex), clipName.c_str());
			}
		}
	}
	}*/

	if (pClip && pClip->GetTags())
	{
		int footcount = 0;
		int rightcount = 0;
		//create an iterator to find the start and end phases of the loop tags
		crTagIterator it(*pClip->GetTags());
		while (*it)
		{
			const crTag* pTag = *it;
			if (pTag->GetKey() == CClipEventTags::Foot)
			{
				bool right = false;
				const crPropertyAttributeBool* attribRight = static_cast<const crPropertyAttributeBool*>((*it)->GetProperty().GetAttribute(CClipEventTags::Right.GetHash()));
				if (attribRight && attribRight->GetBool() == true)
				{
					right = true;
					rightcount++;
				}

				bool heel = false;
				const crPropertyAttributeBool* attribHeel = static_cast<const crPropertyAttributeBool*>((*it)->GetProperty().GetAttribute(CClipEventTags::Heel.GetHash()));
				if (attribHeel && attribHeel->GetBool() == true)
				{
					heel = true;
				}

				if (!attribHeel)
				{
					Displayf("FFS. Toe Tag : %s, %s = %d\n", fwAnimManager::GetName(strLocalIndex(dictIndex)), clipName.c_str(), pTag->GetKey() );
				}

				if (pTag->GetDuration() > 0.01f)
				{
					Displayf("FFS Foot Tag with duration (%6.4f): %s, %s = %d \n", pTag->GetDuration(), fwAnimManager::GetName(strLocalIndex(dictIndex)), clipName.c_str(), pTag->GetKey());
				}
				footcount++;
			}
			++it;
		}

		if (footcount>0)
		{
			Displayf("footcount %d = rightcount %d : %s/%s\n", footcount, rightcount, fwAnimManager::GetName(strLocalIndex(dictIndex)), pClip->GetName());
		}

		if (Abs(footcount-(2*rightcount)) > 1)
		{
			Displayf("************ footcount %d = rightcount %d : %s/%s\n", footcount, rightcount, fwAnimManager::GetName(strLocalIndex(dictIndex)), pClip->GetName());
		}
		}
}

void CAnimViewer::CheckClipAnimDuration(rage::crClip* pClip, atString& clipName, const char* directory)
{
	if (pClip)
	{
		if ( pClip->GetDuration() < 0.01f )
		{
			animDisplayf("CheckClipAndAnimDurations. Clip %s/%s duration = %6.4f.\n", directory, clipName.c_str(), pClip->GetDuration());
		}

		if (pClip->GetType()==crClip::kClipTypeAnimation)
		{
			crClipAnimation* pClipAnim = static_cast<crClipAnimation*>(pClip);
			if(pClipAnim)
			{
				const crAnimation *pAnim =  pClipAnim->GetAnimation();					
				if (pAnim)
				{
					if ( pAnim->GetDuration() < 0.01f )
					{
						animDisplayf("CheckClipAndAnimDurations. Clip %s/%s duration = %6.4f.\n", directory, clipName.c_str(), pClip->GetDuration());
					}

					static float tolerance = 0.01f;
					if ( fabsf(pAnim->GetDuration() - pClip->GetDuration()) > tolerance )
					{
						animDisplayf("CheckClipAndAnimDurations. Clip %s/%s Clip duration (%6.4f) != Anim duration (%6.4f).\n", directory, clipName.c_str(), pClip->GetDuration(), pAnim->GetDuration());
					}
				}
			}
		}
	}
}


void CAnimViewer::CheckTagsRange(rage::crClip* pClip, atString& clipName, const char* directory)
{
	if (pClip)
	{
		crTagIterator it(*pClip->GetTags());
		while (*it)
		{
			const crTag* pTag = *it;
			if (pTag)
			{
				if (pTag->GetStart() < 0.0f ||  pTag->GetStart() > 1.0f)
				{
					animDisplayf("CheckTagsAreInRange. Clip %s/%s. Tag %s. Start = %6.4f.\n", directory, clipName.c_str(), pTag->GetName(), pTag->GetStart());
				}

				if (pTag->GetEnd() < 0.0f ||  pTag->GetEnd() > 1.0f)
				{
					animDisplayf("CheckTagsAreInRange. Clip %s/%s. Tag %s. End = %6.4f.\n", directory, clipName.c_str(), pTag->GetName(), pTag->GetStart());
				}

				if (pTag->GetStart() > pTag->GetEnd())
				{
					animDisplayf("CheckTagsAreInRange. Clip %s/%s. Tag %s. Start (%6.4f) is greater than End (%6.4f)\n", directory, clipName.c_str(), pTag->GetName(), pTag->GetStart(), pTag->GetEnd());
				}
			}
			++it;
		}
	}
}

void CAnimViewer::CheckIkRoot(crAnimation* pAnim, atString& clipName)
{
	crAnimTrack* pTrack = pAnim->FindTrack(rage::kTrackBoneRotation, BONETAG_FACING_DIR);

	if (pTrack)
	{
		bool bOutputAnimName = true;

		QuatV q;
		Vec3V e;

		for (int frame = 0; frame < pAnim->GetNumInternalFrames(); ++frame)
		{
			float t = float(frame);

			pTrack->EvaluateQuaternion(t, q);

			e = QuatVToEulersXYZ(q);
			e *= ScalarVFromF32(RtoD);

			if ((fabsf(e[0]) >= 1.0f) || (fabsf(e[1]) >= 1.0f))
			{
				if (bOutputAnimName)
				{
					Displayf("clip[%s] Has non zero XY values for IK_ROOT:", clipName.c_str());
					bOutputAnimName = false;
				}

				Printf("Value[%d]:\t%f %f %f\n", frame, e[0], e[1], e[2]);
			}
		}
	}
}

void CAnimViewer::CheckAnimDataMoverOnly(crAnimation* pAnim, atString& clipName)
{
	// ROTATION
	crAnimTrack* pRotTrack = pAnim->FindTrack(rage::kTrackMoverRotation, BONETAG_ROOT);

	if (pRotTrack)
	{
		bool bOutputAnimName = true;

		QuatV q;

		for (int frame = 0; frame < pAnim->GetNumInternalFrames(); ++frame)
		{
			float t = float(frame);

			pRotTrack->EvaluateQuaternion(t, q);

			unsigned int iIsCloseAll = IsCloseAll(MagSquared(q), ScalarV(V_ONE), ScalarV(V_FLT_SMALL_2));

			if (!iIsCloseAll)
			{
				if (bOutputAnimName)
				{
					Displayf("clip[%s] Has rotation that is not pure:", clipName.c_str());
					bOutputAnimName = false;
				}

				Printf("Value[%d]: <%f, %f, %f, %f>\n", frame, q.GetXf(), q.GetYf(), q.GetZf(), q.GetWf());
			}
		}
	}

	// TRANSLATION
	crAnimTrack* pTransTrack = pAnim->FindTrack(rage::kTrackMoverTranslation, BONETAG_ROOT);

	if (pTransTrack)
	{
		bool bOutputAnimName = true;

		Vec3V v;

		for (int frame = 0; frame < pAnim->GetNumInternalFrames(); ++frame)
		{
			float t = float(frame);

			pTransTrack->EvaluateVector3(t, v);

			bool bIsFiniteAll = IsFiniteAll(v);

			if (!bIsFiniteAll)
			{
				if (bOutputAnimName)
				{
					Displayf("clip[%s] Has translation that is not valid:", clipName.c_str());
					bOutputAnimName = false;
				}

				Printf("Value[%d]: <%f, %f, %f>\n", frame, v.GetXf(), v.GetYf(), v.GetZf());
			}
		}
	}
}

void CAnimViewer::CheckConstraintHelper(crAnimation* pAnim, atString& clipName)
{
	crAnimTrack* pTrack = pAnim->FindTrack(rage::kTrackBoneTranslation, m_ConstraintHelperValidation.m_bRight ? (u16)BONETAG_CH_R_HAND : (u16)BONETAG_CH_L_HAND);

	if (pTrack)
	{
		bool bOutputAnimName = true;

		Vec3V v;

		for (int frame = 0; frame < pAnim->GetNumInternalFrames(); ++frame)
		{
			float t = float(frame);

			pTrack->EvaluateVector3(t, v);

			if (IsLessThanAll(Mag(v), ScalarV(m_ConstraintHelperValidation.m_fTolerance)))
			{
				if (bOutputAnimName)
				{
					Displayf("clip[%s] Has translation within tolerance:", clipName.c_str());
					bOutputAnimName = false;
				}

				if (m_bVerbose)
				{
					Printf("Value[%d]: <%f, %f, %f>\n", frame, v.GetXf(), v.GetYf(), v.GetZf());
				}
			}
		}
	}
}

bool CAnimViewer::UpdateAudioRelevantWeightThreshold(crClip* pClip, atString& clipName)
{
	static const CClipEventTags::Key audioKey("Audio");
	static const CClipEventTags::Key relevantWeightThresholdKey("RelevantWeightThreshold", 0x368D9C33);

	bool bClipUpdated = false;

	if (pClip)
	{
		crTagIterator it(*pClip->GetTags());

		while (*it)
		{
			crTag* pTag = const_cast<crTag*>(*it);

			if (pTag)
			{
				if ((pTag->GetKey() == audioKey) && (pTag->GetStart() == 0.0f))
				{
					crPropertyAttribute* pAttribute = pTag->GetAttribute(relevantWeightThresholdKey.GetHash());

					if (pAttribute && (pAttribute->GetType() == crPropertyAttribute::kTypeFloat))
					{
						crPropertyAttributeFloat* pAttributeFloat = static_cast<crPropertyAttributeFloat*>(pAttribute);

						if (m_bVerbose)
						{
							Displayf("clip[%s] Setting RelevantWeightThreshold from %5.3f to 0.0", clipName.c_str(), pAttributeFloat->GetFloat());
						}

						pAttributeFloat->SetFloat(0.0f);
						bClipUpdated = true;
					}
				}
			}

			++it;
		}
	}

	return bClipUpdated;
}

void CAnimViewer::CheckForAudioTags(crClip* pClip, atString& clipName)
{
	static const CClipEventTags::Key audioKey("Audio");

	if (pClip)
	{
		bool bAudioTagFound = false;

		crTagIterator it(*pClip->GetTags());

		while (*it)
		{
			crTag* pTag = const_cast<crTag*>(*it);

			if (pTag)
			{
				if (pTag->GetKey() == audioKey)
				{
					bAudioTagFound = true;
					break;
				}
			}

			++it;
		}

		if (!bAudioTagFound)
		{
			Displayf("clip[%s] No Audio tag found", clipName.c_str());
		}
	}
}

bool CAnimViewer::AddTrackIKRoot(crAnimation* pAnim, atString& clipName)
{
	// If animation contains a PELVIS track, assume it contains lower body information and check whether it already contains a facing direction track
	if (pAnim->HasTrack(rage::kTrackBoneRotation, BONETAG_PELVIS) && !pAnim->HasTrack(rage::kTrackBoneRotation, BONETAG_FACING_DIR))
	{
		Displayf("clip[%s] Adding facing direction track", clipName.c_str());

		// Add rotation track for facing direction (Bone IK_ROOT)
		crAnimTrack* pTrack = rage_new crAnimTrack(rage::kTrackBoneRotation, BONETAG_FACING_DIR);

		if (pTrack)
		{
			// Create empty quats
			u32 nFrames = pAnim->GetNumInternalFrames();

			atArray<QuatV> aqValues(nFrames, nFrames);
			for (int frame = 0; frame < aqValues.GetCount(); ++frame)
			{
				aqValues[frame] = QuatV(V_IDENTITY);
			}

			pTrack->CreateQuaternion(aqValues, DEFAULT_ANIM_TOLERANCE, pAnim->GetNumInternalFramesPerChunk());
			
			if (pAnim->CreateTrack(pTrack))
			{
				return true;
			}

			Displayf("clip[%s] Failed to add facing direction track", clipName.c_str());
			delete pTrack;
		}
	}

	return false;
}

bool CAnimViewer::AddTrackIKHead(crAnimation* pAnim, atString& clipName)
{
	animAssert(pAnim);

	// If animation contains a HEAD track, assume it contains head information and check whether it already contains a look direction track
	if (pAnim->HasTrack(rage::kTrackBoneRotation, BONETAG_HEAD) && !pAnim->HasTrack(rage::kTrackBoneRotation, BONETAG_LOOK_DIR))
	{
		Displayf("clip[%s] Adding look direction track", clipName.c_str());

		// Add rotation track for look direction (Bone IK_HEAD)
		crAnimTrack* pTrack = rage_new crAnimTrack(rage::kTrackBoneRotation, BONETAG_LOOK_DIR);

		if (pTrack)
		{
			// Create empty quats
			u32 nFrames = pAnim->GetNumInternalFrames();

			atArray<QuatV> aqValues(nFrames, nFrames);
			for (int frame = 0; frame < aqValues.GetCount(); ++frame)
			{
				aqValues[frame] = QuatV(V_IDENTITY);
			}

			pTrack->CreateQuaternion(aqValues, DEFAULT_ANIM_TOLERANCE, pAnim->GetNumInternalFramesPerChunk());
			
			if (pAnim->CreateTrack(pTrack))
			{
				return true;
			}

			Displayf("clip[%s] Failed to add look direction track", clipName.c_str());
			delete pTrack;
		}
	}

	return false;
}

bool CAnimViewer::AddTrackSpineStabilizationWeight(crAnimation* pAnim, atString& clipName)
{
	animAssert(pAnim);

	const u16 kWeightID = 0;

	if (pAnim->HasTrack(rage::kTrackBoneRotation, BONETAG_ROOT) && pAnim->HasTrack(rage::kTrackBoneRotation, BONETAG_SPINE_ROOT) && !pAnim->HasTrack(kTrackUpperBodyFixupWeight, kWeightID))
	{
		Displayf("clip[%s] Adding spine stabilization weight track", clipName.c_str());

		// Add weight track for kWeightID
		crAnimTrack* pTrack = rage_new crAnimTrack(kTrackUpperBodyFixupWeight, kWeightID);

		if (pTrack)
		{
			// Create empty floats
			u32 nFrames = pAnim->GetNumInternalFrames();

			atArray<float> afValues(nFrames, nFrames);

			for (int frame = 0; frame < afValues.GetCount(); ++frame)
			{
				afValues[frame] = 0.0f;
			}

			pTrack->CreateFloat(afValues, DEFAULT_ANIM_TOLERANCE, pAnim->GetNumInternalFramesPerChunk());
			
			if (pAnim->CreateTrack(pTrack))
			{
				return true;
			}

			Displayf("clip[%s] Failed to add spine stabilization weight track", clipName.c_str());
			delete pTrack;
		}
	}
	else if (pAnim->HasTrack(kTrackUpperBodyFixupWeight, kWeightID) && !pAnim->HasTrack(rage::kTrackBoneRotation, BONETAG_ROOT) && !pAnim->HasTrack(rage::kTrackBoneRotation, BONETAG_SPINE_ROOT))
	{
		Displayf("clip[%s] Removing spine stabilization weight track", clipName.c_str());

		int weightTrackIdx = -1;

		crAnimTrack* pTrack = pAnim->FindTrackIndex(kTrackUpperBodyFixupWeight, kWeightID, weightTrackIdx);

		if (pTrack)
		{
			return pAnim->DeleteTrack(weightTrackIdx);
		}
	}

	return false;
}

bool CAnimViewer::AddTrackRootHeight(crClip* pClip, crAnimation* pAnim, atString& clipName)
{
	animAssert(pClip);
	animAssert(pAnim);

	const u16 kRootHeightID = 0;

	if (pAnim->HasTrack(rage::kTrackBoneRotation, BONETAG_ROOT) && !pAnim->HasTrack(kTrackRootHeight, kRootHeightID))
	{
		Displayf("clip[%s] Adding root height track", clipName.c_str());

		// Add kTrackRootHeight track for kRootHeightID
		crAnimTrack* pTrack = rage_new crAnimTrack(kTrackRootHeight, kRootHeightID);

		if (pTrack)
		{
			float fRootHeight = 0.0f;

			// Get default root height
			const crProperties* pProperties = pClip->GetProperties();

			if (pProperties)
			{
				const crProperty* pProperty = pProperties->FindProperty("Skelfile_DO_NOT_RESOURCE");

				if (pProperty && (pProperty->GetNumAttributes() > 0))
				{
					const crPropertyAttribute* pAttribute = pProperty->GetAttribute("Skelfile_DO_NOT_RESOURCE");

					if (pAttribute && (pAttribute->GetType() == crPropertyAttribute::kTypeString))
					{
						atString skeletonPath("tool:/etc/config/anim/skeletons/");
						skeletonPath += static_cast<const crPropertyAttributeString*>(pAttribute)->GetValue();

						crSkeletonData* pSkeletonData = crSkeletonData::AllocateAndLoad(skeletonPath);

						if (pSkeletonData)
						{
							int boneIdx = -1;

							if (pSkeletonData->HasBoneIds() && pSkeletonData->ConvertBoneIdToIndex((u16)BONETAG_ROOT, boneIdx))
							{
								fRootHeight = pSkeletonData->GetDefaultTransform(boneIdx).GetCol3().GetZf();
							}

							delete pSkeletonData;
						}
					}
				}
			}

			// Initialize frames
			u32 nFrames = pAnim->GetNumInternalFrames();
			atArray<float> afValues(nFrames, nFrames);

			for (int frame = 0; frame < afValues.GetCount(); ++frame)
			{
				afValues[frame] = fRootHeight;
			}

			pTrack->CreateFloat(afValues, DEFAULT_ANIM_TOLERANCE, pAnim->GetNumInternalFramesPerChunk());

			if (pAnim->CreateTrack(pTrack))
			{
				return true;
			}

			Displayf("clip[%s] Failed to add root height track", clipName.c_str());
			delete pTrack;
		}
	}

	return false;
}

bool CAnimViewer::AddTrackHighHeelControl(crAnimation* pAnim, atString& clipName)
{
	animAssert(pAnim);

	const u16 kHighHeelControlID = 15570;

	// If animation contains a PELVIS track, assume it contains lower body information and check whether it already contains a high heel control track
	if (pAnim->HasTrack(rage::kTrackBoneRotation, BONETAG_PELVIS) && !pAnim->HasTrack(rage::kTrackGenericControl, kHighHeelControlID))
	{
		Displayf("clip[%s] Adding high heel control track", clipName.c_str());

		// Add kTrackGenericControl track for high heel
		crAnimTrack* pTrack = rage_new crAnimTrack(rage::kTrackGenericControl, kHighHeelControlID);

		if (pTrack)
		{
			// Initialize frames
			u32 nFrames = pAnim->GetNumInternalFrames();
			atArray<float> afValues(nFrames, nFrames);

			const float fDefaultValue = m_bDefaultHighHeelControlToOne ? 1.0f : 0.0f;

			for (int frame = 0; frame < afValues.GetCount(); ++frame)
			{
				afValues[frame] = fDefaultValue;
			}

			pTrack->CreateFloat(afValues, DEFAULT_ANIM_TOLERANCE, pAnim->GetNumInternalFramesPerChunk());

			if (pAnim->CreateTrack(pTrack))
			{
				return true;
			}

			Displayf("clip[%s] Failed to add high heel track", clipName.c_str());
			delete pTrack;
		}
	}

	return false;
}

bool CAnimViewer::AddTrackFirstPersonCameraWeight(crAnimation *pAnim, atString &clipName)
{
	animAssert(pAnim);

	const u16 kWeightID = 0;

	if (pAnim->HasTrack(rage::kTrackBoneRotation, BONETAG_ROOT) && pAnim->HasTrack(rage::kTrackBoneRotation, BONETAG_SPINE_ROOT) && !pAnim->HasTrack(kTrackFirstPersonCameraWeight, kWeightID))
	{
		Displayf("clip[%s] Adding first person camera weight track", clipName.c_str());

		// Add weight track for kWeightID
		crAnimTrack* pTrack = rage_new crAnimTrack(kTrackFirstPersonCameraWeight, kWeightID);

		if (pTrack)
		{
			// Create empty floats
			u32 nFrames = pAnim->GetNumInternalFrames();

			atArray<float> afValues(nFrames, nFrames);

			float fWeight = pAnim->HasTrack(rage::kTrackCameraLimit, BONETAG_FIRSTPERSONCAM_MINX) ? 1.0f : 0.0f;

			for (int frame = 0; frame < afValues.GetCount(); ++frame)
			{
				afValues[frame] = fWeight;
			}

			pTrack->CreateFloat(afValues, DEFAULT_ANIM_TOLERANCE, pAnim->GetNumInternalFramesPerChunk());

			if (pAnim->CreateTrack(pTrack))
			{
				return true;
			}

			Displayf("clip[%s] Failed to add first person camera weight track", clipName.c_str());
			delete pTrack;
		}
	}

	return false;
}

struct TagPhase
{
	float fStart;
	float fEnd;
};

bool CAnimViewer::AddTagsLookIK(crClip* pClip, atString& clipName)
{
	animAssert(pClip);

	crTags* pTags = pClip->GetTags();

	if (pTags)
	{
		rage::crTag* pNewTag = NULL;
		bool bTagAdded = false;
		bool bFullClipTagAdded = false;

		atArray<TagPhase> aTagsToAdd;

		// Replace HeadIk tags with equivalent LookIk tag
		const u32 headIkKey  = crProperty::StringKey::CalcKey("HeadIk");

		for (int tagIndex = 0; tagIndex < pTags->GetNumTags(); ++tagIndex)
		{
			crTag* pTag = pTags->GetTag(tagIndex);
			crProperty& rProperty = pTag->GetProperty();

			if (rProperty.GetKey() == headIkKey)
			{
				// Remove duplicate full clip HeadIk tags in the process
				bool bFullClipTag = (pTag->GetStart() == 0.0f) && (pTag->GetEnd() == 1.0f);

				if (!bFullClipTag || !bFullClipTagAdded)
				{
					bFullClipTagAdded |= bFullClipTag;

					TagPhase& tagToAdd = aTagsToAdd.Grow();
					tagToAdd.fStart = pTag->GetStart();
					tagToAdd.fEnd = pTag->GetEnd();
				}
			}
		}

		if (aTagsToAdd.GetCount() > 0)
		{
			for (int tagToAddIndex = 0; tagToAddIndex < aTagsToAdd.GetCount(); ++tagToAddIndex)
			{
				if (pNewTag == NULL)
				{
					// Create default LookIk tag
					pNewTag = rage_new crTag();

					if (pNewTag == NULL)
						continue;

					pNewTag->GetProperty().SetName("LookIk");

					crPropertyAttribute* pNewAttrib = crPropertyAttribute::Allocate(crPropertyAttribute::kTypeHashString);
					if(pNewAttrib)
					{
						pNewAttrib->SetName("LookIkTurnSpeed");
						static_cast<crPropertyAttributeHashString*>(pNewAttrib)->GetHashString() = atHashString("Normal");
						pNewTag->GetProperty().AddAttribute(*pNewAttrib);
						delete pNewAttrib;
						pNewAttrib = NULL;
					}

					pNewAttrib = crPropertyAttribute::Allocate(crPropertyAttribute::kTypeHashString);
					if(pNewAttrib)
					{
						pNewAttrib->SetName("LookIkBlendInSpeed");
						static_cast<crPropertyAttributeHashString*>(pNewAttrib)->GetHashString() = atHashString("Normal");
						pNewTag->GetProperty().AddAttribute(*pNewAttrib);
						delete pNewAttrib;
						pNewAttrib = NULL;
					}

					pNewAttrib = crPropertyAttribute::Allocate(crPropertyAttribute::kTypeHashString);
					if(pNewAttrib)
					{
						pNewAttrib->SetName("LookIkBlendOutSpeed");
						static_cast<crPropertyAttributeHashString*>(pNewAttrib)->GetHashString() = atHashString("Normal");
						pNewTag->GetProperty().AddAttribute(*pNewAttrib);
						delete pNewAttrib;
						pNewAttrib = NULL;
					}

					pNewAttrib = crPropertyAttribute::Allocate(crPropertyAttribute::kTypeHashString);
					if(pNewAttrib)
					{
						pNewAttrib->SetName("EyeIkReferenceDirection");
						static_cast<crPropertyAttributeHashString*>(pNewAttrib)->GetHashString() = atHashString("Facing Direction");
						pNewTag->GetProperty().AddAttribute(*pNewAttrib);
						delete pNewAttrib;
						pNewAttrib = NULL;
					}

					pNewAttrib = crPropertyAttribute::Allocate(crPropertyAttribute::kTypeHashString);
					if(pNewAttrib)
					{
						pNewAttrib->SetName("HeadIkReferenceDirection");
						static_cast<crPropertyAttributeHashString*>(pNewAttrib)->GetHashString() = atHashString("Facing Direction");
						pNewTag->GetProperty().AddAttribute(*pNewAttrib);
						delete pNewAttrib;
						pNewAttrib = NULL;
					}

					pNewAttrib = crPropertyAttribute::Allocate(crPropertyAttribute::kTypeHashString);
					if(pNewAttrib)
					{
						pNewAttrib->SetName("NeckIkReferenceDirection");
						static_cast<crPropertyAttributeHashString*>(pNewAttrib)->GetHashString() = atHashString("Facing Direction");
						pNewTag->GetProperty().AddAttribute(*pNewAttrib);
						delete pNewAttrib;
						pNewAttrib = NULL;
					}

					pNewAttrib = crPropertyAttribute::Allocate(crPropertyAttribute::kTypeHashString);
					if(pNewAttrib)
					{
						pNewAttrib->SetName("HeadIkYawDeltaLimit");
						static_cast<crPropertyAttributeHashString*>(pNewAttrib)->GetHashString() = atHashString("Wide");
						pNewTag->GetProperty().AddAttribute(*pNewAttrib);
						delete pNewAttrib;
						pNewAttrib = NULL;
					}

					pNewAttrib = crPropertyAttribute::Allocate(crPropertyAttribute::kTypeHashString);
					if(pNewAttrib)
					{
						pNewAttrib->SetName("HeadIkPitchDeltaLimit");
						static_cast<crPropertyAttributeHashString*>(pNewAttrib)->GetHashString() = atHashString("Wide");
						pNewTag->GetProperty().AddAttribute(*pNewAttrib);
						delete pNewAttrib;
						pNewAttrib = NULL;
					}

					pNewAttrib = crPropertyAttribute::Allocate(crPropertyAttribute::kTypeHashString);
					if(pNewAttrib)
					{
						pNewAttrib->SetName("HeadIkAttitude");
						static_cast<crPropertyAttributeHashString*>(pNewAttrib)->GetHashString() = atHashString("Full");
						pNewTag->GetProperty().AddAttribute(*pNewAttrib);
						delete pNewAttrib;
						pNewAttrib = NULL;
					}

					pNewAttrib = crPropertyAttribute::Allocate(crPropertyAttribute::kTypeHashString);
					if(pNewAttrib)
					{
						pNewAttrib->SetName("NeckIkYawDeltaLimit");
						static_cast<crPropertyAttributeHashString*>(pNewAttrib)->GetHashString() = atHashString("Wide");
						pNewTag->GetProperty().AddAttribute(*pNewAttrib);
						delete pNewAttrib;
						pNewAttrib = NULL;
					}

					pNewAttrib = crPropertyAttribute::Allocate(crPropertyAttribute::kTypeHashString);
					if(pNewAttrib)
					{
						pNewAttrib->SetName("NeckIkPitchDeltaLimit");
						static_cast<crPropertyAttributeHashString*>(pNewAttrib)->GetHashString() = atHashString("Narrow");
						pNewTag->GetProperty().AddAttribute(*pNewAttrib);
						delete pNewAttrib;
						pNewAttrib = NULL;
					}

					pNewAttrib = crPropertyAttribute::Allocate(crPropertyAttribute::kTypeHashString);
					if(pNewAttrib)
					{
						pNewAttrib->SetName("TorsoIkYawDeltaLimit");
						static_cast<crPropertyAttributeHashString*>(pNewAttrib)->GetHashString() = atHashString("Disabled");
						pNewTag->GetProperty().AddAttribute(*pNewAttrib);
						delete pNewAttrib;
						pNewAttrib = NULL;
					}

					pNewAttrib = crPropertyAttribute::Allocate(crPropertyAttribute::kTypeHashString);
					if(pNewAttrib)
					{
						pNewAttrib->SetName("TorsoIkPitchDeltaLimit");
						static_cast<crPropertyAttributeHashString*>(pNewAttrib)->GetHashString() = atHashString("Disabled");
						pNewTag->GetProperty().AddAttribute(*pNewAttrib);
						delete pNewAttrib;
						pNewAttrib = NULL;
					}

					pNewAttrib = crPropertyAttribute::Allocate(crPropertyAttribute::kTypeHashString);
					if(pNewAttrib)
					{
						pNewAttrib->SetName("TorsoIkLeftArm");
						static_cast<crPropertyAttributeHashString*>(pNewAttrib)->GetHashString() = atHashString("Disabled");
						pNewTag->GetProperty().AddAttribute(*pNewAttrib);
						delete pNewAttrib;
						pNewAttrib = NULL;
					}

					pNewAttrib = crPropertyAttribute::Allocate(crPropertyAttribute::kTypeHashString);
					if(pNewAttrib)
					{
						pNewAttrib->SetName("TorsoIkRightArm");
						static_cast<crPropertyAttributeHashString*>(pNewAttrib)->GetHashString() = atHashString("Disabled");
						pNewTag->GetProperty().AddAttribute(*pNewAttrib);
						delete pNewAttrib;
						pNewAttrib = NULL;
					}
				}

				if (pNewTag)
				{
					TagPhase& tagToAdd = aTagsToAdd[tagToAddIndex];

					pNewTag->SetStart(tagToAdd.fStart);
					pNewTag->SetEnd(tagToAdd.fEnd);

					pTags->AddTag(*pNewTag);
				}
			}

			Displayf("clip[%s] Replaced %d HeadIk tag(s)", clipName.c_str(), aTagsToAdd.GetCount());
			bTagAdded = true;
		}

		if (pNewTag)
		{
			delete pNewTag;
		}

		pTags->RemoveAllTags((crTag::Key)headIkKey);

		return bTagAdded;
	}

	return false;
}

bool CAnimViewer::ResetIkRoot(crAnimation* pAnim, atString& clipName)
{
	crAnimTrack* pTrack = pAnim->FindTrack(rage::kTrackBoneRotation, BONETAG_FACING_DIR);

	if (pTrack)
	{
		QuatV q;
		Vec3V e;

		for (int frame = 0; frame < pAnim->GetNumInternalFrames(); ++frame)
		{
			float t = float(frame);

			pTrack->EvaluateQuaternion(t, q);

			e = QuatVToEulersXYZ(q);
			e *= ScalarVFromF32(RtoD);

			if ((fabsf(e[0]) >= 1.0f) || (fabsf(e[1]) >= 1.0f))
			{
				Displayf("clip[%s] Resetting IK_ROOT", clipName.c_str());

				int ikRootTrackIdx = -1;

				pAnim->FindTrackIndex(rage::kTrackBoneRotation, BONETAG_FACING_DIR, ikRootTrackIdx);
				
				if (pAnim->DeleteTrack(ikRootTrackIdx))
				{
					return AddTrackIKRoot(pAnim, clipName);
				}
			}
		}
	}

	return false;
}

bool CAnimViewer::DeleteTrackArmRoll(crAnimation* pAnim, atString& clipName)
{
	static const eAnimBoneTag aBoneTags[] =
	{
		BONETAG_L_ARMROLL,
		BONETAG_R_ARMROLL,
		BONETAG_L_FOREARMROLL,
		BONETAG_R_FOREARMROLL,
		BONETAG_INVALID
	};

	bool bAnimUpdated = false;
	int boneTagIndex = 0;
	int trackIndex;

	while (aBoneTags[boneTagIndex] != BONETAG_INVALID)
	{
		crAnimTrack* pTrack = pAnim->FindTrackIndex(rage::kTrackBoneRotation, (u16)aBoneTags[boneTagIndex], trackIndex);

		if (pTrack)
		{
			if (m_bVerbose && !bAnimUpdated)
			{
				Displayf("clip[%s] Deleted found arm roll bone tracks", clipName.c_str());
			}

			bAnimUpdated |= pAnim->DeleteTrack(trackIndex);
		}

		boneTagIndex++;
	};

	return bAnimUpdated;
}

bool CAnimViewer::ConvertFirstPersonData(crClip* pClip, crAnimation* pAnim, atString& clipName)
{
	int rotationTrackIndex;
	int translationTrackIndex;

	crAnimTrack* pRotationTrack = pAnim->FindTrackIndex(rage::kTrackBoneRotation, BONETAG_R_IK_HAND, rotationTrackIndex);
	crAnimTrack* pTranslationTrack = pAnim->FindTrackIndex(rage::kTrackBoneTranslation, BONETAG_R_IK_HAND, translationTrackIndex);

	crAnimTrack* pHelperRotationTrack = pAnim->FindTrack(rage::kTrackBoneRotation, BONETAG_CH_R_HAND);

	if (pRotationTrack && pTranslationTrack && !pHelperRotationTrack)
	{
		QuatV qDefault(V_IDENTITY);
		Vec3V vDefault(V_ZERO);

		const crProperties* pProperties = pClip->GetProperties();

		if (pProperties)
		{
			const crProperty* pProperty = pProperties->FindProperty("Skelfile_DO_NOT_RESOURCE");

			if (pProperty && (pProperty->GetNumAttributes() > 0))
			{
				const crPropertyAttribute* pAttribute = pProperty->GetAttribute("Skelfile_DO_NOT_RESOURCE");

				if (pAttribute && (pAttribute->GetType() == crPropertyAttribute::kTypeString))
				{
					atString skeletonPath("tool:/etc/config/anim/skeletons/");
					skeletonPath += static_cast<const crPropertyAttributeString*>(pAttribute)->GetValue();

					crSkeletonData* pSkeletonData = crSkeletonData::AllocateAndLoad(skeletonPath);

					if (pSkeletonData)
					{
						int boneIdx = -1;

						if (pSkeletonData->HasBoneIds() && pSkeletonData->ConvertBoneIdToIndex((u16)BONETAG_R_IK_HAND, boneIdx))
						{
							if (boneIdx >= 0)
							{
								const Mat34V& mtxDefault = pSkeletonData->GetDefaultTransform(boneIdx);
								qDefault = QuatVFromMat34V(mtxDefault);
								vDefault = mtxDefault.GetCol3();
							}
						}

						delete pSkeletonData;
					}
				}
			}
		}

		static ScalarV vTolerance(0.10f);
		Vec3V vValue(V_ZERO);
		bool bConvert = false;

		const u32 nFrames = pAnim->GetNumInternalFrames();

		for (u32 frame = 0; frame < nFrames; ++frame)
		{
			pTranslationTrack->EvaluateVector3((float)frame, vValue);

			if (IsGreaterThanAll(Mag(vValue), vTolerance))
			{
				if (m_bVerbose)
				{
					Displayf("clip[%s] Converting BONETAG_R_IK_HAND to BONETAG_CH_R_HAND", clipName.c_str());
				}
				bConvert = true;
				break;
			}
		}

		if (bConvert)
		{
			const u32 nFramesPerChunk = pAnim->GetNumInternalFramesPerChunk();

			// Copy BONETAG_R_IK_HAND rotation track to BONETAG_CH_R_HAND
			crAnimTrack* pClonedRotationTrack = pRotationTrack->Clone();
			pClonedRotationTrack->SetId((u16)BONETAG_CH_R_HAND);
			pAnim->DeleteTrack(rotationTrackIndex);

			// Create BONETAG_R_IK_HAND rotation track with default values 
			crAnimTrack* pDefaultRotationTrack = rage_new crAnimTrack(rage::kTrackBoneRotation, BONETAG_R_IK_HAND);

			if (pDefaultRotationTrack)
			{
				// Create empty quats
				atArray<QuatV> aqValues(nFrames, nFrames);
				for (int frame = 0; frame < aqValues.GetCount(); ++frame)
				{
					aqValues[frame] = qDefault;
				}

				pDefaultRotationTrack->CreateQuaternion(aqValues, DEFAULT_ANIM_TOLERANCE, nFramesPerChunk);
			
				if (!pAnim->CreateTrack(pDefaultRotationTrack))
				{
					delete pDefaultRotationTrack;
				}
			}

			// Copy BONETAG_R_IK_HAND translation track to BONETAG_CH_R_HAND
			pTranslationTrack = pAnim->FindTrackIndex(rage::kTrackBoneTranslation, BONETAG_R_IK_HAND, translationTrackIndex);
			crAnimTrack* pClonedTranslationTrack = pTranslationTrack->Clone();
			pClonedTranslationTrack->SetId((u16)BONETAG_CH_R_HAND);
			pAnim->DeleteTrack(translationTrackIndex);

			// Create BONETAG_R_IK_HAND translation track with default values 
			crAnimTrack* pDefaultTranslationTrack = rage_new crAnimTrack(rage::kTrackBoneTranslation, BONETAG_R_IK_HAND);

			if (pDefaultTranslationTrack)
			{
				// Create empty vectors
				atArray<Vec3V> avValues(nFrames, nFrames);
				for (int frame = 0; frame < avValues.GetCount(); ++frame)
				{
					avValues[frame] = vDefault;
				}

				pDefaultTranslationTrack->CreateVector3(avValues, DEFAULT_ANIM_TOLERANCE, nFramesPerChunk);
			
				if (!pAnim->CreateTrack(pDefaultTranslationTrack))
				{
					delete pDefaultTranslationTrack;
				}
			}

			// Create tracks
			if (!pAnim->CreateTrack(pClonedRotationTrack))
			{
				delete pClonedRotationTrack;
			}

			if (!pAnim->CreateTrack(pClonedTranslationTrack))
			{
				delete pClonedTranslationTrack;
			}

			return true;
		}
	}

	return false;
}

void CAnimViewer::BatchProcessFindAndReplace()
{
	animDisplayf("Begin Find and Replace:");

	CClipFileIterator iter(MakeFunctorRet(CAnimViewer::ProcessFindAndReplace), m_BatchProcessInputFolder, true);
	iter.Iterate();

	animDisplayf("Find and Replace complete.");
}

bool CAnimViewer::ProcessFindAndReplace(crClip* pClip, fiFindData UNUSED_PARAM(fileData), const char* UNUSED_PARAM(folderName))
{
	atString clipName(pClip->GetName());
	clipName.Lowercase();

	u32 uNbOccurrences = 0;

	// Check clip properties
	crProperties* pProperties = pClip->GetProperties();

	if (pProperties)
	{
		crProperty* pProperty = pProperties->FindProperty(m_FindAndReplaceData.propertyName);

		if (pProperty && (pProperty->GetNumAttributes() > 0))
		{
			crPropertyAttribute* pAttribute = pProperty->GetAttribute(m_FindAndReplaceData.attributeName);

			if (pAttribute)
			{
				if (pAttribute->GetType() == crPropertyAttribute::kTypeString)
				{
					atString& stringValue = static_cast<crPropertyAttributeString*>(pAttribute)->GetString();

					if (stringValue == m_FindAndReplaceData.findWhat)
					{
						stringValue = m_FindAndReplaceData.replaceWith;
						uNbOccurrences++;
					}
				}
			}
		}
	}

	// Check clip tags
	crTags* pTags = pClip->GetTags();

	if (pTags)
	{
		u32 propertyNameKey  = crProperty::StringKey::CalcKey(m_FindAndReplaceData.propertyName);
		u32 attributeNameKey = crProperty::StringKey::CalcKey(m_FindAndReplaceData.attributeName);

		for (int tagIndex = 0; tagIndex < pTags->GetNumTags(); ++tagIndex)
		{
			crTag* pTag = pTags->GetTag(tagIndex);
			crProperty& rProperty = pTag->GetProperty();

			if (rProperty.GetKey() == propertyNameKey)
			{
				crPropertyAttribute* pAttribute = rProperty.GetAttribute(attributeNameKey);

				if (pAttribute)
				{
					// TODO: Add support for other types if needed (kTypeFloat, kTypeInt, kTypeBool, etc.)
					if (pAttribute->GetType() == crPropertyAttribute::kTypeString)
					{
						atString& stringValue = static_cast<crPropertyAttributeString*>(pAttribute)->GetString();

						if (stringValue == m_FindAndReplaceData.findWhat)
						{
							stringValue = m_FindAndReplaceData.replaceWith;
							uNbOccurrences++;
						}
					}
				}
			}
		}
	}

	if (uNbOccurrences > 0)
	{
		Displayf("clip[%s] Replaced %u occurrence(s)", clipName.c_str(), uNbOccurrences);
		return true;
	}

	return false;
}

void CAnimViewer::PrintClipDictionariesWithMemoryGroup()
{
	Printf("Memory Group : %s\n", fwClipSetManager::GetMemoryGroupNames()[ms_iSelectedMemoryGroup]);
	atHashString memoryGroup(fwClipSetManager::GetMemoryGroupNames()[ms_iSelectedMemoryGroup]);
	int clipDictionaryCount= fwAnimManager::GetSize();
	for (int i = 0; i<clipDictionaryCount; i++)
	{
		// Is the current slot valid
		if (fwAnimManager::IsValidSlot(strLocalIndex(i)))
		{
			// Is the dictionary in the image
			if ( CStreaming::IsObjectInImage(strLocalIndex(i), fwAnimManager::GetStreamingModuleId()) )
			{
				const char* p_dictName = fwAnimManager::GetName(strLocalIndex(i));
				const fwClipDictionaryMetadata *pClipDictionaryMetadata = fwClipSetManager::GetClipDictionaryMetadata(p_dictName);
				if (pClipDictionaryMetadata)
				{	
					if (memoryGroup == pClipDictionaryMetadata->GetMemoryGroup())
					{
						Printf("%s\n", p_dictName);
					}
				}
			}
		}
	}
}

void CAnimViewer::PrintClipDictionariesReferencedByAnyCipSet()
{
	int clipDictionaryCount= fwAnimManager::GetSize();
	for (int i = 0; i<clipDictionaryCount; i++)
	{
		// Is the current slot valid
		if (fwAnimManager::IsValidSlot(strLocalIndex(i)))
		{
			// IS the dictionary in the image
			if ( CStreaming::IsObjectInImage(strLocalIndex(i), fwAnimManager::GetStreamingModuleId()) )
			{

				bool isClipDictionaryReferencedByAnyCipSet = false;
				int clipSetCount = fwClipSetManager::GetClipSetCount();
				if(clipSetCount > 0)
				{
					for(int clipSetIndex = 0; clipSetIndex < clipSetCount; clipSetIndex++)
					{
						// fwMvClipSetId clipSetId = fwClipSetManager::GetClipSetIdByIndex(clipSetIndex);
						const fwClipSet *clipSet = fwClipSetManager::GetClipSetByIndex(clipSetIndex);

						if (clipSet->GetClipDictionaryIndex() == i)
						{
							isClipDictionaryReferencedByAnyCipSet = true;
						}
					}
				}

				if (isClipDictionaryReferencedByAnyCipSet)
				{
					Displayf("Is Referenced : %s/\n", fwAnimManager::GetName(strLocalIndex(i)));
				}
				else
				{
					Displayf("Is Not Referenced : %s\n", fwAnimManager::GetName(strLocalIndex(i)));
				}
			}
		}
	}
}


void CMotionTreeVisualiserSimpleIterator::Visit(crmtNode& node)
{
	crmtIterator::Visit(node);

	if (node.GetNodeType()==crmtNode::kNodeClip)
	{
		crmtNodeClip* pClipNode = static_cast<crmtNodeClip*>(&node);
		if (pClipNode 
			&& pClipNode->GetClipPlayer().HasClip())
		{
			//get the weight from the parent node (if it exists)
			float weight = CalcVisibleWeight(node);
			//if (weight > 0.0f)
			{

				const crClip* clip = pClipNode->GetClipPlayer().GetClip();
				/* To display the clip dictionary name we need to find the clip dictionary index in the clip dictionary store */
				const char *clipDictionaryName = NULL;

				if (clip->GetDictionary())
				{
					/* Iterate through the clip dictionaries in the clip dictionary store */
					const crClipDictionary *clipDictionary = NULL;
					int clipDictionaryIndex = 0, clipDictionaryCount = g_ClipDictionaryStore.GetSize();
					for(; clipDictionaryIndex < clipDictionaryCount && clipDictionaryName==NULL; clipDictionaryIndex ++)
					{
						if(g_ClipDictionaryStore.IsValidSlot(strLocalIndex(clipDictionaryIndex)))
						{
							clipDictionary = g_ClipDictionaryStore.Get(strLocalIndex(clipDictionaryIndex));
							if(clipDictionary == clip->GetDictionary())
							{
								clipDictionaryName = g_ClipDictionaryStore.GetName(strLocalIndex(clipDictionaryIndex));
							}
						}
					}
				}

				char outText[256] = "";

				const crClip* pClip = pClipNode->GetClipPlayer().GetClip();
				Assert(pClip);
				atString clipName(pClip->GetName());
				clipName.Replace("pack:/", "");
				clipName.Replace(".clip", "");

				atString userName;
				const crProperties *pProperties = pClip->GetProperties();
				if(pProperties)
				{
					static u32 uUserKey = ATSTRINGHASH("SourceUserName", 0x69497CB4);
					const crProperty *pProperty = pProperties->FindProperty(uUserKey);
					if(pProperty)
					{
						const crPropertyAttribute *pPropertyAttribute = pProperty->GetAttribute(uUserKey);
						if(pPropertyAttribute && pPropertyAttribute->GetType() == crPropertyAttribute::kTypeInt)
						{
							const crPropertyAttributeInt *pPropertyAttributeInt = static_cast< const crPropertyAttributeInt * >(pPropertyAttribute);

							int iUserHash = pPropertyAttributeInt->GetInt();
							u32 uUserHash = *reinterpret_cast< u32 * >(&iUserHash);
							const char *szUser = atHashString(uUserHash).TryGetCStr();
							if(szUser)
							{
								userName = szUser;
							}
							else
							{
								userName = atVarString("%u", uUserHash);
							}
						}
					}
				}

				sprintf(outText, "%-8.3f %-40s %-32s %-8.3f %-8.3f %-8.3f %-20s", 
					weight,
					clipDictionaryName ? clipDictionaryName : "UNKNOWN", 
					clipName.c_str(), 
					pClipNode->GetClipPlayer().GetTime(),
					pClipNode->GetClipPlayer().GetRate(),
					pClip->GetDuration()>0.0f ? pClipNode->GetClipPlayer().GetTime() / pClip->GetDuration() : 0.0f,
					userName.c_str());

				if (CAnimViewer::m_bDumpMotionTreeSimple)
				{
					Displayf("\ndict name = %s, anim name = %s, weight = %.3f, time = %.3f, rate = %.3f, phase = %.3f",
						clipDictionaryName ? clipDictionaryName : "UNKNOWN", 
						clipName.c_str(), 
						weight,
						pClipNode->GetClipPlayer().GetTime(),
						pClipNode->GetClipPlayer().GetRate(),
						pClip->GetDuration()>0.0f ? pClipNode->GetClipPlayer().GetTime() / pClip->GetDuration() : 0.0f );

					if (clipDictionaryName)
					{
						const char *pLocalPath = CDebugClipDictionary::FindLocalDirectoryForDictionary(clipDictionaryName);
						if(strlen(pLocalPath)>0)
						{
							char clipFileNameAndPath[512];
							sprintf(clipFileNameAndPath, "%s\\%s.clip", pLocalPath, clipName.c_str());
							//Displayf("Successfully found %s\n", clipFileNameAndPath);

							crClip* pLoadedClip = crClip::AllocateAndLoad(clipFileNameAndPath, NULL);
							if (pLoadedClip)
							{
								crProperties* pProps = pLoadedClip->GetProperties();
								if (pProps)
								{
									crProperty* pProp = pProps->FindProperty(crProperties::CalcKey("Approval_DO_NOT_RESOURCE"));
									if (pProp)
									{
										atHashString propertyHashString = static_cast<crPropertyAttributeHashString*>(pProp->GetAttribute("Status"))->GetHashString();
										int iDateDay = static_cast<crPropertyAttributeInt*>(pProp->GetAttribute("Day"))->GetInt();
										int iDateMonth = static_cast<crPropertyAttributeInt*>(pProp->GetAttribute("Month"))->GetInt();
										int iDateYear = static_cast<crPropertyAttributeInt*>(pProp->GetAttribute("Year"))->GetInt();

										Displayf("     Approval = %s (%d/%d/%d), ", propertyHashString.GetCStr(), iDateDay, iDateMonth, iDateYear);
									}

									pProp = pProps->FindProperty(crProperties::CalcKey("Compressionfile_DO_NOT_RESOURCE"));
									if (pProp)
									{
										atString propertyString = static_cast<crPropertyAttributeString*>(pProp->GetAttribute("Compressionfile_DO_NOT_RESOURCE"))->GetString();
										Displayf("     Compressionfile = %s", propertyString.c_str());
									}

									pProp = pProps->FindProperty(crProperties::CalcKey("FBXFile_DO_NOT_RESOURCE"));
									if (pProp)
									{
										atString propertyString = static_cast<crPropertyAttributeString*>(pProp->GetAttribute("FBXFile_DO_NOT_RESOURCE"))->GetString();
										Displayf("     FBXFile  = %s", propertyString.c_str());
									}

									pProp = pProps->FindProperty(crProperties::CalcKey("ControlFile_DO_NOT_RESOURCE"));
									if (pProp)
									{
										atString propertyString = static_cast<crPropertyAttributeString*>(pProp->GetAttribute("ControlFile_DO_NOT_RESOURCE"))->GetString();
										Displayf("     ControlFile  = %s", propertyString.c_str());
									}

									pProp = pProps->FindProperty(crProperties::CalcKey("SourceMachineName_DO_NOT_RESOURCE"));
									if (pProp)
									{
										atString propertyString = static_cast<crPropertyAttributeString*>(pProp->GetAttribute("SourceMachineName_DO_NOT_RESOURCE"))->GetString();
										Displayf("     SourceMachineName = %s", propertyString.c_str());
									}

									pProp = pProps->FindProperty(crProperties::CalcKey("Additive_DO_NOT_RESOURCE"));
									if (pProp)
									{
										int propertyInt = static_cast<crPropertyAttributeInt*>(pProp->GetAttribute("Additive_DO_NOT_RESOURCE"))->GetInt();
										Displayf("     Additive = %d", propertyInt);
									}

									pProp = pProps->FindProperty(crProperties::CalcKey("Looping_DO_NOT_RESOURCE"));
									if (pProp)
									{
										int propertyInt = static_cast<crPropertyAttributeInt*>(pProp->GetAttribute("Looping_DO_NOT_RESOURCE"))->GetInt();
										Displayf("     Looping = %d", propertyInt);
									}

									pProp = pProps->FindProperty(crProperties::CalcKey("Mover_DO_NOT_RESOURCE"));
									if (pProp)
									{
										int propertyInt = static_cast<crPropertyAttributeInt*>(pProp->GetAttribute("Mover_DO_NOT_RESOURCE"))->GetInt();
										Displayf("     Mover = %d", propertyInt);
									}

									pProp = pProps->FindProperty(crProperties::CalcKey("Skelfile_DO_NOT_RESOURCE"));
									if (pProp)
									{
										int propertyInt = static_cast<crPropertyAttributeInt*>(pProp->GetAttribute("Skelfile_DO_NOT_RESOURCE"))->GetInt();
										Displayf("     Skelfile = %d", propertyInt);
									}

									pProp = pProps->FindProperty(crProperties::CalcKey("UsedInNMBlend_DO_NOT_RESOURCE"));
									if (pProp)
									{
										int propertyInt = static_cast<crPropertyAttributeInt*>(pProp->GetAttribute("UsedInNMBlend_DO_NOT_RESOURCE"))->GetInt();
										Displayf("     UsedInNMBlend = %d", propertyInt);
									}


								}

								delete pLoadedClip;
							}
						}
					}
				}

				Color32 colour = CRGBA(255,192,96,255);

				if (CAnimViewer::m_bVisualiseMotionTreeSimple)
				{
					Vector2 vTextRenderPos(CAnimViewer::m_fHorizontalBorder, CAnimViewer::m_fVerticalBorder + (m_noOfTexts)*CAnimViewer::m_fVertDist);
					vTextRenderPos.y = CAnimViewer::m_fVerticalBorder + (m_noOfTexts++)*CAnimViewer::m_fVertDist;
					grcDebugDraw::Text( vTextRenderPos, colour, outText);
				}
			}
		}
	}
}

float CMotionTreeVisualiserSimpleIterator::CalcVisibleWeight(crmtNode& node)
{
	crmtNode * pNode = &node;

	float weight = 1.0f;

	crmtNode* pParent = pNode->GetParent();

	while (pParent)
	{
		float outWeight = 1.0f;

		switch(pParent->GetNodeType())
		{
		case crmtNode::kNodeBlend:
			{
				crmtNodePairWeighted* pPair = static_cast<crmtNodePairWeighted*>(pParent);

				if (pNode==pPair->GetFirstChild())
				{
					outWeight = 1.0f - pPair->GetWeight();
				}
				else
				{
					outWeight = pPair->GetWeight();
				}
			}
			break;
		case crmtNode::kNodeBlendN:
			{
				crmtNodeN* pN = static_cast<crmtNodeN*>(pParent);
				u32 index = pN->GetChildIndex(*pNode);
				outWeight = pN->GetWeight(index);
			}
			break;
		default:
			outWeight = 1.0f;
			break;
		}

		weight*=outWeight;

		pNode = pParent;
		pParent = pNode->GetParent();
	}

	return weight;
}

//////////////////////////////////////////////////////////////////////////

void CAnimViewer::VisualiseMotionTreeSimple()
{
	if (m_pDynamicEntity && m_pDynamicEntity->GetAnimDirector())
	{	
		char outText[256];
		Color32 colour = CRGBA(255,192,96,255);
		Vector2 vTextRenderPos(CAnimViewer::m_fHorizontalBorder, CAnimViewer::m_fVerticalBorder + CAnimViewer::m_fVertDist);
		vTextRenderPos.y = CAnimViewer::m_fVerticalBorder + CAnimViewer::m_fVertDist;
		sprintf(outText, "%-8s %-40s %-32s %-8s %-8s %-8s %-20s", "Weight", "Dict name", "Anim name", "Time", "Rate", "Phase", "User");
		grcDebugDraw::Text( vTextRenderPos, colour, outText);
		CMotionTreeVisualiserSimpleIterator it(NULL);
		it.m_noOfTexts++;
		it.m_noOfTexts++;
		m_pDynamicEntity->GetAnimDirector()->GetMove().GetMotionTree().Iterate(it);

		fwAnimDirectorComponentMotionTree* pComponentMotionTreeMidPhysics = m_pDynamicEntity->GetAnimDirector()->GetComponentByPhase<fwAnimDirectorComponentMotionTree>(fwAnimDirectorComponent::kPhaseMidPhysics);
		if (pComponentMotionTreeMidPhysics)
		{
			crmtNode *pRootNode = pComponentMotionTreeMidPhysics->GetMotionTree().GetRootNode();
			if (pRootNode)
			{
				it.Start();
				it.Iterate(*pRootNode);
				it.Finish();
			}
	}
}
}

//////////////////////////////////////////////////////////////////////////
//	Motion tree visualiser implementation
//////////////////////////////////////////////////////////////////////////

void CAnimViewer::VisualiseMotionTree()
{
#if CR_DEV
	if (m_pDynamicEntity && m_bVisualiseMotionTree && m_pDynamicEntity->GetAnimDirector())
	{
		crmtMotionTree* pTree = NULL;
		fwAnimDirectorComponentMotionTree* pMotionTreeComponent = static_cast<fwAnimDirectorComponentMotionTree*>(m_pDynamicEntity->GetAnimDirector()->GetComponentByTypeAndPhase(fwAnimDirectorComponent::kComponentTypeMotionTree, m_eVisualiseMotionTreePhase));
		if (pMotionTreeComponent != NULL)
		{
			pTree = &pMotionTreeComponent->GetMotionTree();
		}

		if (pTree != NULL)
		{
			if (m_bVisualiseMotionTreeNewStyle && pTree == &m_pDynamicEntity->GetAnimDirector()->GetMove().GetMotionTree())
			{
				float fOriginX = g_motionTreeVisualisationXIndent, fOriginY = g_motionTreeVisualisationYIndent;
				fwMoveDumpRenderFlags renderFlags(fwMotionTreeVisualiser::ms_bRenderTree,
					fwMotionTreeVisualiser::ms_bRenderClass,
					fwMotionTreeVisualiser::ms_bRenderWeight,
					fwMotionTreeVisualiser::ms_bRenderLocalWeight,
					fwMotionTreeVisualiser::ms_bRenderStateActiveName,
					fwMotionTreeVisualiser::ms_bRenderRequests,
					fwMotionTreeVisualiser::ms_bRenderFlags,
					fwMotionTreeVisualiser::ms_bRenderInputParams,
					fwMotionTreeVisualiser::ms_bRenderOutputParams,
					fwMotionTreeVisualiser::ms_bRenderInvalidNodes,
					fwMotionTreeVisualiser::ms_bRenderSynchronizerWeights,
					fwMotionTreeVisualiser::ms_bRenderSynchronizerGraph,
					fwMotionTreeVisualiser::ms_bRenderDofs,
					fwMotionTreeVisualiser::ms_bRenderAddress);

				if(m_MoveDump->GetFrameCount() > 0)
				{
					if(m_bCaptureMotionTree)
					{
						u32 uFrameIndex = m_MoveDump->GetFrameCount() - 1;

						m_MoveDump->Render(uFrameIndex, fOriginX, fOriginY, g_motionTreeVisualisationXIncrement, g_motionTreeVisualisationYIncrement, renderFlags);
					}
					else
					{
						u32 uFrameIndex = Min(m_uRenderMotionTreeCaptureFrame, m_MoveDump->GetFrameCount() - 1);

						m_MoveDump->Render(uFrameIndex, fOriginX, fOriginY, g_motionTreeVisualisationXIncrement, g_motionTreeVisualisationYIncrement, renderFlags);
					}
				}
				else
				{
					m_MoveDump->Render(0, fOriginX, fOriginY, g_motionTreeVisualisationXIncrement, g_motionTreeVisualisationYIncrement, renderFlags);
				}
			}
			else
			{
				fwMotionTreeVisualiser visualiser;
				visualiser.m_pDynamicEntity = m_pDynamicEntity;
				pTree->Iterate(visualiser);
			}
		}
	}
#endif
}

void fwMotionTreeTester::PreVisit(crmtNode& ) 
{
	m_Depth++;
	m_MaxDepth = Max(m_Depth, m_MaxDepth);
}

// PURPOSE: Iterator visit override
void fwMotionTreeTester::Visit(crmtNode&) { m_complexity++; m_Depth--; }

void fwMotionTreeVisualiser::Start()
{
	// reset some internal variables
	m_drawPosition.x = g_motionTreeVisualisationXIndent;
	m_drawPosition.y  = g_motionTreeVisualisationYIndent;
}

void fwMotionTreeVisualiser::RenderText(const char * text, float nodeWeight)
{

	Color32 color;
	bool renderBackground;

	if (nodeWeight < 0.001f)
	{	
		TUNE_GROUP_FLOAT ( MOTION_TREE_RENDER, fNoWeightAlpha,0.5f, 0.0f,1.0f,0.001f);
		TUNE_GROUP_FLOAT ( MOTION_TREE_RENDER, fNoWeightColor,1.0f, 0.0f,1.0f,0.001f);
		// this is effectively blended out - render grey with no background
		color.Setf(fNoWeightColor, fNoWeightColor, fNoWeightColor, fNoWeightAlpha);
		renderBackground = false;
	}
	else
	{
		TUNE_GROUP_FLOAT ( MOTION_TREE_RENDER, fRedChannel,1.0f, 1.0f,1.0f,0.001f);
		TUNE_GROUP_FLOAT ( MOTION_TREE_RENDER, fGreenChannel,1.0f, 0.0f,1.0f,0.001f);
		TUNE_GROUP_FLOAT ( MOTION_TREE_RENDER, fBlueChannel,0.0f, 0.0f,1.0f,0.001f);
		TUNE_GROUP_FLOAT ( MOTION_TREE_RENDER, fWeightedBaseAlpha, 0.5f, 0.0f,1.0f,0.001f);

		color.Setf( fRedChannel, fGreenChannel, fBlueChannel, fWeightedBaseAlpha + nodeWeight*(1.0f-fWeightedBaseAlpha) );
		renderBackground = true;
	}

	grcDebugDraw::Text( m_drawPosition, color, text, renderBackground );
}

void fwMotionTreeVisualiser::PreVisit(crmtNode& node)
{
	m_drawPosition.x+= g_motionTreeVisualisationXIncrement;
	crmtIterator::PreVisit(node);

	Vector2 screenPos;
	screenPos = m_drawPosition;

	switch(node.GetNodeType())
	{
	case crmtNode::kNodeBlend:
	case crmtNode::kNodeAddSubtract:
		VisualisePair(node, screenPos);
		break;
	case crmtNode::kNodeMerge:
		VisualiseMerge(node, screenPos);
		break;
	case crmtNode::kNodeBlendN:
	case crmtNode::kNodeAddN:
	case crmtNode::kNodeMergeN:
		VisualiseN(node, screenPos);
		break;
	case crmtNode::kNodeStateMachine:
	case crmtNode::kNodeReference:
	case crmtNode::kNodeStateRoot:
	case crmtNode::kNodeSubNetwork:
		VisualiseState(node, screenPos);
		break;
	case crmtNode::kNodeFilter:
		VisualiseFilter(node, screenPos);
		break;
	case crmtNode::kNodeExpression:
		VisualiseExpression(node, screenPos);
		break;
	case crmtNode::kNodeIk:
		RenderText("IK", CalcVisibleWeight(&node) );
		m_drawPosition.y+= g_motionTreeVisualisationYIncrement;
		break;
	default:
		//do nothing
		break;
	}	
}

// PURPOSE: Iterator visit override
void fwMotionTreeVisualiser::Visit(crmtNode& node)
{	
	crmtIterator::Visit(node);
	
	Vector2 screenPos;
	screenPos = m_drawPosition;			//the starting width of the text on screen
	
	switch(node.GetNodeType())
	{
	case crmtNode::kNodeClip:
		VisualiseClip(node, screenPos);
		break;
	case crmtNode::kNodeAnimation:
		VisualiseAnimation(node, screenPos);
		break;
	case crmtNode::kNodeProxy:
		//TODO - add the ability to visualise proxy nodes more usefully
		RenderText("Proxy", CalcVisibleWeight(&node) );
		m_drawPosition.y+= g_motionTreeVisualisationYIncrement;
		break;
	case crmtNode::kNodeInvalid:
		if (ms_bRenderInvalidNodes)
		{
			RenderText("Invalid", CalcVisibleWeight(&node) );
			m_drawPosition.y+= g_motionTreeVisualisationYIncrement;
		}
		break;
	case crmtNode::kNodeIdentity:
		RenderText("Identity", CalcVisibleWeight(&node) );
		m_drawPosition.y+= g_motionTreeVisualisationYIncrement;
		break;
	case crmtNode::kNodePose:
		RenderText("Pose", CalcVisibleWeight(&node) );
		m_drawPosition.y+= g_motionTreeVisualisationYIncrement;
		break;
	case crmtNode::kNodeFrame:
		RenderText("Frame", CalcVisibleWeight(&node) );
		m_drawPosition.y+= g_motionTreeVisualisationYIncrement;
		break;
	case crmtNode::kNodePm:
		VisualisePM(node, screenPos);
		break;
	default:
		//do nothing
		break;
	}	

	m_drawPosition.x-= g_motionTreeVisualisationXIncrement;
}

void fwMotionTreeVisualiser::VisualiseClip(crmtNode& node, Vector2 UNUSED_PARAM(screenPos))
{
	crmtNodeClip* pClipNode = static_cast<crmtNodeClip*>(&node);
	char outText[256];
	FastString finalText;

	const crClipPlayer& player = pClipNode->GetClipPlayer();
	const crClip* clip = player.GetClip();
	if (clip)
	{
#if __BANK
		/* To display the clip dictionary name we need to find the clip dictionary index in the clip dictionary store */
		const char *clipDictionaryName = NULL;

		if (clip->GetDictionary())
		{
		/* Iterate through the clip dictionaries in the clip dictionary store */
		const crClipDictionary *clipDictionary = NULL;
		int clipDictionaryIndex = 0, clipDictionaryCount = g_ClipDictionaryStore.GetSize();
			for(; clipDictionaryIndex < clipDictionaryCount && clipDictionaryName==NULL; clipDictionaryIndex ++)
		{
			if(g_ClipDictionaryStore.IsValidSlot(strLocalIndex(clipDictionaryIndex)))
			{
				clipDictionary = g_ClipDictionaryStore.Get(strLocalIndex(clipDictionaryIndex));
					if(clipDictionary == clip->GetDictionary())
				{
						clipDictionaryName = g_ClipDictionaryStore.GetName(strLocalIndex(clipDictionaryIndex));
					}
				}
			}
		}
#endif // __BANK

		float weight = 0.0f;
		//get the weight from the parent node (if it exists)
		if (GetWeightFromParent(node, weight))
		{
			sprintf(outText, "-W:%.3f ", weight );
			finalText+=outText;
		}

		float phase = 0.0f;
		float lastPhase = 0.0f;

		FastString clipName(clip->GetName());
		clipName.Replace("pack:/", "");
		clipName.Replace(".clip", "");

#if __BANK
		sprintf(outText, "%s/%s, T:%.3f/%.3f", clipDictionaryName ? clipDictionaryName : "UNKNOWN", clipName.c_str(), player.GetTime(), clip->GetDuration());
#else // __BANK
		sprintf(outText, "%s, T:%.3f/%.3f", clipName.c_str(), player.GetTime(), clip->GetDuration());
#endif // __BANK
		finalText+=outText;
		if (clip->GetDuration()>0.0f)
		{
			phase = player.GetTime() / clip->GetDuration();
			lastPhase = phase - player.GetDelta() / clip->GetDuration();

			sprintf(outText, ", P:%.3f", phase);
			finalText+=outText;
		}
		sprintf(outText, ", R:%.3f", player.GetRate());
		finalText+=outText;

		sprintf(outText, ", L:%s", player.IsLooped() ? "Yes" : "No" );
		finalText+=outText;

		phase = Clamp(phase, 0.0f, 1.0f);
		lastPhase = Clamp(lastPhase, 0.0f, 1.0f);

		GetEventTagText(finalText, clip, lastPhase, phase);

		RenderText(finalText.c_str(), CalcVisibleWeight(&node));
		m_drawPosition.y+= g_motionTreeVisualisationYIncrement;
	}
	else
	{
		float weight = 0.0f;
		//get the weight from the parent node (if it exists)
		if (GetWeightFromParent(node, weight))
		{
			sprintf(outText, "-W:%.3f ", weight );
			sprintf(outText, "%s NULL clip", outText);
		}
		else
		{
			sprintf(outText, "NULL clip");
		}

		RenderText(outText, CalcVisibleWeight(&node));
		m_drawPosition.y+= g_motionTreeVisualisationYIncrement;
	}

	//Visualise any observers attached to the node
	static const u32 s_msgId = ATSTRINGHASH("MotionTreeVisualise", 0xB6EA596);

	if (ms_bRenderSynchronizerWeights || ms_bRenderSynchronizerGraph)
	{
		atString str;

		crmtMessage msg(s_msgId, (void*)(&str));

		pClipNode->MessageObservers(msg);

		if (str.GetLength()>0)
		{	
			m_drawPosition.x += g_motionTreeVisualisationXIncrement;
			
			if (ms_bRenderSynchronizerWeights)
			{

				atArray<atString> results;
				str.Split(results, "\n", true);
				float visibleWeight = CalcVisibleWeight(&node);
				for (int i=0; i<results.GetCount(); i++)
				{
					RenderText(results[i].c_str(), visibleWeight);
					m_drawPosition.y+= g_motionTreeVisualisationYIncrement;
				}
			}

			if (ms_bRenderSynchronizerGraph)
			{
				const crClip * pClip = pClipNode->GetClipPlayer().GetClip();

//#if __DEV
//				atArray<crmtSynchronizerTag::SyncTagObserver*> observerList;
//				static const u32 s_msgTagSyncId = ATSTRINGHASH("MotionTreeVisualiseTagSync", 0x8F142FF);
//				crmtMessage msg(s_msgTagSyncId, (void*)(&observerList));
//#endif //__DEV

				pClipNode->MessageObservers(msg);

				if (pClip)
				{
					//draw the tag graph for this clip:
					TUNE_GROUP_FLOAT(MOTION_TREE_RENDER, fTagSyncGraphXOffset, 0.3f, 0.0f, 1.0f, 0.0001f);
					TUNE_GROUP_FLOAT(MOTION_TREE_RENDER, fTagSyncGraphWidth, 0.3f, 0.0f, 1.0f, 0.0001f);
					TUNE_GROUP_FLOAT(MOTION_TREE_RENDER, fTagSyncGraphHeight, 0.007f, 0.0f, 1.0f, 0.00001f);
					TUNE_GROUP_FLOAT(MOTION_TREE_RENDER, fTagSyncGraphReferenceYOffset, 0.014f, 0.0f, 1.0f, 0.00001f);
					float fPhase = pClip->ConvertTimeToPhase(pClipNode->GetClipPlayer().GetTime());
					float fGraphCentre = m_drawPosition.x + fTagSyncGraphXOffset;
					float fGraphCentreY = m_drawPosition.y+fTagSyncGraphHeight/2.0f;
					int maxObserverCount = 0;

					Vector2 vRangeStart(fGraphCentre - (fTagSyncGraphWidth*fPhase) ,fGraphCentreY);
					Vector2 vRangeEnd(fGraphCentre + ((1.0f - fPhase)*fTagSyncGraphWidth) ,fGraphCentreY);

					// draw the current phase marker
					grcDebugDraw::Line(Vector2(fGraphCentre, fGraphCentreY), Vector2(fGraphCentre, fGraphCentreY - 0.01f), Color_green1);
					// draw the range
					grcDebugDraw::Line(vRangeStart, vRangeEnd, Color_grey);
					// draw the tag positions
					if ( pClip->GetTags())
					{
						crTagIterator it(*pClip->GetTags(), 0.0f, 1.0f, CClipEventTags::Foot.GetHash());
						while (*it)
						{
							float fTagCentre = vRangeStart.x + ((*it)->GetMid()*fTagSyncGraphWidth);
							Color32 colour;
							const crPropertyAttributeBool* attribRight = static_cast<const crPropertyAttributeBool*>((*it)->GetProperty().GetAttribute(CClipEventTags::Right.GetHash()));
							if (attribRight && attribRight->GetBool() == true)
							{
								colour = Color_red;
							}
							else
							{
								colour = Color_blue;
							}
							grcDebugDraw::Line(Vector2(fTagCentre, fGraphCentreY), Vector2(fTagCentre, fGraphCentreY - 0.01f), colour);

//#if __DEV
//							// draw the reference time underneath
//							for (int i=0; i<observerList.GetCount(); i++)
//							{
//								crmtSynchronizerTag::SyncTagObserver* pObserver = observerList[i];
//								float referencePhase = pObserver->MapPhaseToReference((*it)->GetMid());
//								char buf[8];
//								sprintf(buf, "%.3f", referencePhase);
//								grcDebugDraw::Text(Vector2(fTagCentre, fGraphCentreY + fTagSyncGraphReferenceYOffset*i), Color_black, buf);
//							}
//
//							if (observerList.GetCount()>maxObserverCount)
//								maxObserverCount = observerList.GetCount();
//#endif //__DEV
							++it;
						}
					}
					m_drawPosition.y+= (fTagSyncGraphHeight + fTagSyncGraphReferenceYOffset*maxObserverCount);
				}
			}

			m_drawPosition.x -= g_motionTreeVisualisationXIncrement;
		}
	}
}

void fwMotionTreeVisualiser::GetEventTagText(FastString& text, const crClip* pClip, float startPhase, float endPhase)
{
	if (pClip->GetTags())
	{
		crTagIterator it(*pClip->GetTags(), startPhase, endPhase);

		while (*it)
		{
			const crTag* pTag = *it;
			
			atHashString tagName(pTag->GetKey());

			text += ", ";
			text += tagName.TryGetCStr();

			++it;
		}
	}
}

void fwMotionTreeVisualiser::VisualiseAnimation(crmtNode& node, Vector2 UNUSED_PARAM(screenPos))
{
	crmtNodeAnimation* pNode = static_cast<crmtNodeAnimation*>(&node);
	char outText[256] = "";

	if (pNode->GetAnimPlayer().GetAnimation() )
	{
		float weight = 0.0f;
		//get the weight from the parent node (if it exists)
		if (GetWeightFromParent(node, weight))
		{
			sprintf(outText, "-W:%.3f ", weight );
		}

		const crAnimation* pAnim = pNode->GetAnimPlayer().GetAnimation();

		sprintf(outText, "%s Animation:", outText);

		FastString animName(pAnim->GetName());
		animName.Replace("pack:/", "");
		animName.Replace(".anim", "");

		sprintf(outText, "%s %s, T:%.3f", outText, animName.c_str(), pNode->GetAnimPlayer().GetTime());
		if (pAnim->GetDuration()>0.0f)
		{
			sprintf(outText, "%s, P:%.3f", outText, pNode->GetAnimPlayer().GetTime() / pAnim->GetDuration());
		}
		RenderText(&outText[0], CalcVisibleWeight(pNode) );
		m_drawPosition.y+= g_motionTreeVisualisationYIncrement;
	}
}


void fwMotionTreeVisualiser::VisualiseExpression(crmtNode& node, Vector2 UNUSED_PARAM(screenPos))
{
	crmtNodeExpression* pExpressionNode = static_cast<crmtNodeExpression*>(&node);
	char outText[256];
	FastString finalText;

	const crExpressionPlayer& player = pExpressionNode->GetExpressionPlayer();
	const crExpressions* pExpressions = player.GetExpressions();
	if (pExpressions)
	{
		float weight = 0.0f;
		//get the weight from the parent node (if it exists)
		if (GetWeightFromParent(node, weight))
		{
			sprintf(outText, "-W:%.3f ", weight );
			finalText+=outText;
		}

		FastString expressionsName(pExpressions->GetName());
		expressionsName.Replace("pack:/", "");
		expressionsName.Replace(".expr", "");

#if __BANK
		//sprintf(outText, "%s/%s", expressionDictionaryName, expressionsName.c_str());
		sprintf(outText, "%s", expressionsName.c_str());
#else // __BANK
		sprintf(outText, "%s", expressionsName.c_str());
#endif // __BANK

		finalText+=outText;

		sprintf (outText, ", W : %.3f, T : %.3f", pExpressionNode->GetWeight(), pExpressionNode->GetExpressionPlayer().GetTime());
		finalText+=outText;
		
		RenderText(finalText.c_str(), CalcVisibleWeight(&node));
		m_drawPosition.y+= g_motionTreeVisualisationYIncrement;
	}
	else
	{
		float weight = 0.0f;
		//get the weight from the parent node (if it exists)
		if (GetWeightFromParent(node, weight))
		{
			sprintf(outText, "-W:%.3f ", weight );
			sprintf(outText, "%s NULL expression", outText);
		}
		else
		{
			sprintf(outText, "NULL expression");
		}

		RenderText(outText, CalcVisibleWeight(&node));
		m_drawPosition.y+= g_motionTreeVisualisationYIncrement;
	}

	//Visualise any observers attached to the node
	static const u32 s_msgId = ATSTRINGHASH("MotionTreeVisualise", 0xB6EA596);

	if (ms_bRenderSynchronizerWeights)
	{
		atString str;

		crmtMessage msg(s_msgId, (void*)(&str));

		pExpressionNode->MessageObservers(msg);

		if (str.GetLength()>0)
		{	
			m_drawPosition.x += g_motionTreeVisualisationXIncrement;
			atArray<atString> results;
			str.Split(results, "\n", true);
			float visibleWeight = CalcVisibleWeight(&node);
			for (int i=0; i<results.GetCount(); i++)
			{
				RenderText(results[i].c_str(), visibleWeight);
				m_drawPosition.y+= g_motionTreeVisualisationYIncrement;
			}
			m_drawPosition.x -= g_motionTreeVisualisationXIncrement;
		}
	}
}


void fwMotionTreeVisualiser::VisualisePM(crmtNode& node, Vector2 UNUSED_PARAM(screenPos))
{
	crmtNodePm* pPmNode = static_cast<crmtNodePm*>(&node);
	char outText[256];
	FastString finalText;

	float weight = 0.0f;
	//get the weight from the parent node (if it exists)
	if (GetWeightFromParent(node, weight))
	{
		sprintf(outText, "-W:%.3f ", weight );
	}
	finalText+=outText;

	sprintf(outText, " PM:");
	finalText+=outText;

	crpmParameterizedMotionPlayer &player = pPmNode->GetParameterizedMotionPlayer();
	crpmParameterizedMotion *pm = player.GetParameterizedMotion();
	if(pm)
	{
		const crpmParameterizedMotionData *pmData = pm->GetData();
		if(pmData)
		{
			const char *pmDataName = pmData->GetName();
			if(pmDataName)
			{
				sprintf(outText, " %s", pmDataName);
				finalText+=outText;
			}
		}
	}

	RenderText(finalText.c_str(), CalcVisibleWeight(&node));
	m_drawPosition.y+= g_motionTreeVisualisationYIncrement;
}


void fwMotionTreeVisualiser::VisualisePair(crmtNode& node, Vector2 UNUSED_PARAM(screenPos))
{
	crmtNodePair* pNode = static_cast<crmtNodePair*>(&node);
	char outText[256] = "";

	float weight = 0.0f;
	//get the weight from the parent node (if it exists)
	if (GetWeightFromParent(node, weight))
	{
		sprintf(outText, "-W:%.3f ", weight );
	}

	const char *frameFilterName = "No filter";
	crFrameFilter *frameFilter = pNode->GetFilter();
	if(frameFilter)
	{
		frameFilterName = g_FrameFilterDictionaryStore.FindFrameFilterNameSlow(frameFilter);
		if(!frameFilterName)
		{
			frameFilterName = "Name missing";
		}
	}

	switch(pNode->GetNodeType())
	{
	case crmtNode::kNodeBlend:
		sprintf(outText, "%s Blend %s:", outText, frameFilterName);
		break;
	case crmtNode::kNodeAddSubtract:
		sprintf(outText, "%s Add %s:", outText, frameFilterName);
		break;
	default:
		break;
	}

	RenderText( &outText[0], CalcVisibleWeight(pNode) );
	//we should draw some lines...
	m_drawPosition.y+= g_motionTreeVisualisationYIncrement;
}

void fwMotionTreeVisualiser::VisualiseMerge(crmtNode& node, Vector2 UNUSED_PARAM(screenPos))
{
	crmtNodeMerge* pNode = static_cast<crmtNodeMerge*>(&node);
	char outText[256] = "";

	float weight = 0.0f;
	//get the weight from the parent node (if it exists)
	if (GetWeightFromParent(node, weight))
	{
		sprintf(outText, "-W:%.3f ", weight );
	}

	const char *frameFilterName = "No filter";
	crFrameFilter *frameFilter = pNode->GetFilter();
	if(frameFilter)
	{
		frameFilterName = g_FrameFilterDictionaryStore.FindFrameFilterNameSlow(frameFilter);
		if(!frameFilterName)
		{
			frameFilterName = "Name missing";
		}
	}

	sprintf(outText, "%s Merge %s:", outText, frameFilterName);

	RenderText( &outText[0], CalcVisibleWeight(pNode) );
	//we should draw some lines...
	m_drawPosition.y+= g_motionTreeVisualisationYIncrement;
}

void fwMotionTreeVisualiser::VisualiseN(crmtNode& node, Vector2 UNUSED_PARAM(screenPos))
{
	crmtNodeN* pNode = static_cast<crmtNodeN*>(&node);
	char outText[256] = "";

	float weight = 0.0f;
	//get the weight from the parent node (if it exists)
	if (GetWeightFromParent(node, weight))
	{
		sprintf(outText, "-W:%.3f ", weight );
	}

	switch(pNode->GetNodeType())
	{
	case crmtNode::kNodeBlendN:
		sprintf(outText, "%s BlendN:", outText);
		break;
	case crmtNode::kNodeAddN:
		sprintf(outText, "%s AddN:", outText);
		break;
	case crmtNode::kNodeMergeN:
		sprintf(outText, "%s MergeN:", outText);
		break;
	default:
		break;
	}

	//sprintf(outText, "%s weight:%.3f", outText, pNode->GetWeight());

	RenderText( &outText[0], CalcVisibleWeight(pNode) );
	//don't y increment this one, but we should draw some lines...
	m_drawPosition.y+= g_motionTreeVisualisationYIncrement;
}

void fwMotionTreeVisualiser::VisualiseState(crmtNode& node, Vector2 UNUSED_PARAM(screenPos))
{
	crmtNodeParent* pNode = static_cast<crmtNodeParent*>(&node);
	char outText[256] = "";
	FastString totalString;

	if (IsInterestingStateNode(node))
	{
		float weight = 0.0f;
		//get the weight from the parent node (if it exists)
		if (GetWeightFromParent(node, weight))
		{
			sprintf(outText, "-W:%.3f ", weight );
			totalString+=outText;
		}

#if __DEV
		crmtNode::eNodes nodeType = node.GetNodeTypeInfo().GetNodeType();
		switch(nodeType)
		{
		case crmtNode::kNodeSubNetwork:
			{
				const mvNodeSubNetwork *pInterface = static_cast<const mvNodeSubNetwork*>(pNode);

				const fwAnimDirectorComponentMove* componentMove = static_cast<const fwAnimDirectorComponentMove*>(m_pDynamicEntity->GetAnimDirector()->GetComponentByType(fwAnimDirectorComponent::kComponentTypeMove));
				Assert(componentMove);
				fwMoveNetworkPlayer* pPlayer = componentMove->FindMoveNetworkPlayer(pInterface->DebugGetSubNetwork());

				totalString += "(";
				AppendHashStringToString(totalString, pInterface->DebugGetNetworkId());
				totalString += ") SubNetwork ";
				if (pInterface->DebugGetSubNetwork())
				{
					totalString+= fwAnimDirector::FindNetworkDefName(pInterface->DebugGetSubNetwork()->GetDefinition());
					sprintf(outText, "(%p)", pInterface->DebugGetSubNetwork());
					totalString+=outText;
				}

				if (pPlayer)
				{
					totalString += ", clipset:";
					totalString+= pPlayer->GetClipSetId().GetHash() ? pPlayer->GetClipSetId().GetCStr() : "none";

					if (ms_bRenderInputParams)
					{
						totalString+= " ";
						atString inputString;
						pPlayer->DumpInputParamBuffer(inputString);
						totalString += inputString.c_str();
					}
					if (ms_bRenderOutputParams)
					{
						totalString+= " ";
						atString outputString;
						pPlayer->DumpOutputParamBuffer(outputString);
						totalString += outputString.c_str();
					}

					totalString.Replace("\n", ", ");
				}
				break;
			}

		case crmtNode::kNodeStateMachine:
			{
				const mvNodeStateMachine *pInterface = static_cast<const mvNodeStateMachine*>(pNode);
				totalString += "(";
				AppendHashStringToString(totalString, pInterface->GetDefinition()->m_id);

				totalString += ") State machine";
				const mvNodeStateBaseDef* activeDef = pInterface->GetActiveDef();
				if(activeDef)
				{
					totalString += " (";
					AppendHashStringToString(totalString, activeDef->m_id);
					totalString += ")";
					VisualiseTransitions(activeDef, totalString);
				}
				break;
			}
		
		case crmtNode::kNodeStateRoot:
			{
				const mvNodeStateRoot *pInterface = static_cast<const mvNodeStateRoot*>(pNode);
				totalString += "(";
				AppendHashStringToString(totalString, pInterface->GetDefinition()->m_id);
				totalString += ") Root motion tree";

				if (ms_bRenderInputParams)
				{
					totalString+= " ";
					atString inputString;
					m_pDynamicEntity->GetAnimDirector()->GetMove().DumpInputParamBuffer(inputString);
					totalString += inputString.c_str();
				}
				if (ms_bRenderOutputParams)
				{
					totalString+= " ";
					atString outputString;
					m_pDynamicEntity->GetAnimDirector()->GetMove().DumpOutputParamBuffer(outputString);
					totalString += outputString.c_str();
				}

				totalString.Replace("\n", ", ");
				break;
			}

		default: totalString += " crmtNodeState:";
		}
#endif //__DEV
		//sprintf(outText, "%s weight:%.3f", outText, pNode->GetWeight());

		RenderText( totalString.c_str(), CalcVisibleWeight(pNode) );
		//don't y increment this one, but we should draw some lines...
		m_drawPosition.y+= g_motionTreeVisualisationYIncrement;
	}
}

void fwMotionTreeVisualiser::VisualiseFilter(crmtNode& node, Vector2 UNUSED_PARAM(screenPos))
{
	crmtNodeFilter* pNode = static_cast<crmtNodeFilter*>(&node);
	char outText[256] = "";

	float weight = 0.0f;
	//get the weight from the parent node (if it exists)
	if (GetWeightFromParent(node, weight))
	{
		sprintf(outText, "-W:%.3f ", weight );
	}



	const char *frameFilterName = "No filter";
	crFrameFilter *frameFilter = pNode->GetFilter();
	if(frameFilter)
	{
		frameFilterName = g_FrameFilterDictionaryStore.FindFrameFilterNameSlow(frameFilter);
		if(!frameFilterName)
		{
			frameFilterName = "Name missing";
		}
	}

	sprintf(outText, "%s Filter %s:", outText, frameFilterName);

	RenderText( &outText[0], CalcVisibleWeight(pNode) );

	m_drawPosition.y+= g_motionTreeVisualisationYIncrement;
}

bool fwMotionTreeVisualiser::GetWeightFromParent(crmtNode& node, float& outWeight)
{
	crmtNode* pNode = node.GetParent();

	if (pNode)
	{
		switch(pNode->GetNodeType())
		{
		case crmtNode::kNodeBlend:
		case crmtNode::kNodeAddSubtract:
			{
				crmtNodePairWeighted* pPair = static_cast<crmtNodePairWeighted*>(pNode);

				if (&node==pPair->GetFirstChild())
				{
					outWeight = 1.0f - pPair->GetWeight();
				}
				else
				{
					outWeight = pPair->GetWeight();
				}
			}
			return true;
		case crmtNode::kNodeBlendN:
		case crmtNode::kNodeAddN:
		case crmtNode::kNodeMergeN:
			{
				crmtNodeN* pParent = static_cast<crmtNodeN*>(pNode);
				u32 index = pParent->GetChildIndex(node);
				outWeight = pParent->GetWeight(index);
			}
			return true;
		default:
			return false;
		}
	}
	else
	{
		return false;
	}
}

bool fwMotionTreeVisualiser::IsInterestingStateNode (crmtNode& node)
{

	if (ms_bRenderInvalidNodes)
		return true;

	for (u32 i=0; i<node.GetNumChildren(); i++)
	{
		crmtNode* pChild = node.GetChild(i);

		if (pChild)
		{
			if (pChild->GetNodeType()!=crmtNode::kNodeInvalid)
			{
				return true;
			}
		}
	}

	return false;
}


float fwMotionTreeVisualiser::CalcVisibleWeight(crmtNode * pNode)
{
	float weight = 1.0f;

	crmtNode* pParent = pNode->GetParent();

	while (pParent)
	{
		float outWeight = 1.0f;

		switch(pParent->GetNodeType())
		{
		case crmtNode::kNodeBlend:
			{
				crmtNodePairWeighted* pPair = static_cast<crmtNodePairWeighted*>(pParent);

				if (pNode==pPair->GetFirstChild())
				{
					outWeight = 1.0f - pPair->GetWeight();
				}
				else
				{
					outWeight = pPair->GetWeight();
				}
			}
			break;
		case crmtNode::kNodeBlendN:
			{
				crmtNodeN* pN = static_cast<crmtNodeN*>(pParent);
				u32 index = pN->GetChildIndex(*pNode);
				outWeight = pN->GetWeight(index);
			}
			break;
		default:
			outWeight = 1.0f;
			break;
		}

		weight*=outWeight;

		pNode = pParent;
		pParent = pNode->GetParent();
	}

	return weight;
}

void fwMotionTreeVisualiser::VisualiseTransitions(const mvNodeStateBaseDef* pDef, FastString& string)
{
	if (pDef)
	{
		string += " ";
		AppendU32(string, pDef->m_TransitionCount);
		string += " Transitions";
		
	}
}

void fwMotionTreeVisualiser::AddWidgets(bkBank* pBank)
{
	pBank->PushGroup("Motion tree rendering options", false, NULL);
	{
		pBank->AddToggle("Render tree", &ms_bRenderTree,  NullCB, "Should MoVE tree be rendered in the motion tree visualiser");
		pBank->AddToggle("Render class", &ms_bRenderClass,  NullCB, "Should class be rendered in the motion tree visualiser");
		pBank->AddToggle("Render weight", &ms_bRenderWeight,  NullCB, "Should weight be rendered in the motion tree visualiser");
		pBank->AddToggle("Render local weight", &ms_bRenderLocalWeight,  NullCB, "Should local weight be rendered in the motion tree visualiser");
		pBank->AddToggle("Render state active name", &ms_bRenderStateActiveName,  NullCB, "Should state active name be rendered in the motion tree visualiser");
		pBank->AddToggle("Render requests", &ms_bRenderRequests,  NullCB, "Should requests be rendered in the motion tree visualiser");
		pBank->AddToggle("Render flags", &ms_bRenderFlags,  NullCB, "Should flags be rendered in the motion tree visualiser");
		pBank->AddToggle("Render input parameters", &ms_bRenderInputParams,  NullCB, "Should input parameters be rendered in the motion tree visualiser");
		pBank->AddToggle("Render output parameters", &ms_bRenderOutputParams,  NullCB, "Should output parameters be rendered in the motion tree visualiser");
		pBank->AddToggle("Render invalid nodes", &ms_bRenderInvalidNodes,  NullCB, "Should invalid nodes be rendered in the motion tree visualiser");
		pBank->AddToggle("Render synchronizer weights", &ms_bRenderSynchronizerWeights,  NullCB, "Should the synchronizer target info be rendered in the motion tree visualiser");
		pBank->AddToggle("Render synchronizer graph", &ms_bRenderSynchronizerGraph,  NullCB, "Should the synchronizer graph be rendered in the motion tree visualiser");
		pBank->AddToggle("Render dofs", &ms_bRenderDofs,  NullCB, "Should dofs be rendered in the motion tree visualiser");
		pBank->AddToggle("Render addresses", &ms_bRenderAddress,  NullCB, "Should node addresses be rendered in the motion tree visualiser");
	}
	pBank->PopGroup();
}


FrameEditorWidgets::FrameEditorWidgets()
: m_Frame(NULL)
, m_SkelData(NULL)
, m_ReadOnly(false)
{
}

FrameEditorWidgets::~FrameEditorWidgets()
{
}

void FrameEditorWidgets::Init(crFrame& frame, const crSkeletonData* skelData, bool readOnly)
{
	Assert(!m_Frame);

	m_Frame = &frame;
	m_SkelData = skelData;
	m_ReadOnly = readOnly;
}

void FrameEditorWidgets::Shutdown()
{
	m_WidgetValues.Reset();
	m_WidgetInvalid.Reset();

	m_Frame = NULL;
	m_SkelData = NULL;
	m_ReadOnly = false;
}

class AddWidgetIterator : public crFrameIterator<AddWidgetIterator>
{
public:
	AddWidgetIterator(crFrame& frame, atArray<Vec3V>& widgetValues, atArray<bool>& widgetInvalid, bkGroup& bank, const crSkeletonData* skelData, bool readOnly)
		: crFrameIterator<AddWidgetIterator>(frame)
		, m_WidgetValues(&widgetValues)
		, m_WidgetInvalid(&widgetInvalid)
		, m_Bank(&bank)
		, m_SkelData(skelData)
		, m_ReadOnly(readOnly)
	{
	}

	void IterateDof(const crFrameData::Dof& dof, const crFrame::Dof&, float)
	{
		char buf[512];
		sprintf(buf, "track %d id %d ", dof.m_Track, dof.m_Id);

		char buf2[256]; 
		buf2[0] = '\0';

		const char* name = crSkeletonData::DebugConvertBoneIdToName(dof.m_Id);
		const char* track = crAnimTrack::ConvertTrackIndexToName(dof.m_Track);
		if(name || track)
		{
			if(name)
			{
				sprintf(buf2, "'%s' ", name);
			}
			if(track)
			{
				safecat(buf2, track, 256);
			}
		}

		switch(dof.m_Track)
		{
		case kTrackBoneTranslation:
		case kTrackBoneRotation:
		case kTrackBoneScale:
			{
				int boneIdx;
				if(m_SkelData && m_SkelData->ConvertBoneIdToIndex(dof.m_Id, boneIdx))
				{
					const crBoneData* bd = m_SkelData->GetBoneData(boneIdx);
					if(bd)
					{
						switch(dof.m_Track)
						{
						case kTrackBoneTranslation:
							sprintf(buf2, "bone[%d] '%s' translation", bd->GetIndex(), bd->GetName());
							break;

						case kTrackBoneRotation:
							sprintf(buf2, "bone[%d] '%s' rotation", bd->GetIndex(), bd->GetName());
							break;

						case kTrackBoneScale:
							sprintf(buf2, "bone[%d] '%s' scale", bd->GetIndex(), bd->GetName());
							break;
						}
					}
				}
			}
			break;
		}

		strcat(buf, buf2);
		bkGroup* bkGroup = m_Bank->AddGroup(buf, false);

		bool& b = m_WidgetInvalid->Grow();
		b = true;

		bkGroup->AddToggle("invalid", &b);

		Vec3V& v = m_WidgetValues->Grow();
		v = Vec3V(V_ZERO);

		switch(dof.m_Type)
		{
		case kFormatTypeQuaternion:
			bkGroup->AddAngle("euler x", &v[0], bkAngleType::RADIANS);
			bkGroup->AddAngle("euler y", &v[1], bkAngleType::RADIANS);
			bkGroup->AddAngle("euler z", &v[2], bkAngleType::RADIANS);
			break;

		case kFormatTypeVector3:
			bkGroup->AddVector("vector xyz", reinterpret_cast<Vector3*>(&v), bkVector::FLOAT_MIN_VALUE, bkVector::FLOAT_MAX_VALUE, 0.001f);
			break;

		case kFormatTypeFloat:
			bkGroup->AddSlider("float", &v[0], -5.f, 5.f, 0.001f);
			break;
		}

//		m_Bank->PopGroup();
	}

	atArray<Vec3V>* m_WidgetValues;
	atArray<bool>* m_WidgetInvalid;
	bkGroup* m_Bank;
	const crSkeletonData* m_SkelData;
	bool m_ReadOnly;
};

void FrameEditorWidgets::AddWidgets(bkGroup& bk)
{
	Assert(m_Frame);

	const int numDofs = m_Frame->GetNumDofs();

	m_WidgetValues.Reserve(numDofs);
	m_WidgetInvalid.Reserve(numDofs);

	AddWidgetIterator it(*m_Frame, m_WidgetValues, m_WidgetInvalid, bk, m_SkelData, m_ReadOnly);
	it.Iterate(NULL, 1.f, false);
}

class ResetWidgetIterator : public crFrameIterator<ResetWidgetIterator>
{
public:
	ResetWidgetIterator(crFrame& frame, atArray<Vec3V>& widgetValues, atArray<bool>& widgetInvalid)
		: crFrameIterator<ResetWidgetIterator>(frame)
		, m_WidgetValues(&widgetValues)
		, m_WidgetInvalid(&widgetInvalid)
	{
		m_Index = 0;
	}

	void IterateDof(const crFrameData::Dof& dof, const crFrame::Dof& dest, float)
	{
		bool& b = (*m_WidgetInvalid)[m_Index];
		b = dest.IsInvalid();

		Vec3V& v = (*m_WidgetValues)[m_Index];
		if(!dest.IsInvalid())
		{
			switch(dof.m_Type)
			{
			case kFormatTypeQuaternion:
				{
					QuatV q;
					q = dest.GetUnsafe<QuatV>();
					v = QuatVToEulersXYZ(q);
				}
				break;

			case kFormatTypeVector3:
				v = dest.GetUnsafe<Vec3V>();
				break;

			case kFormatTypeFloat:
				v[0] = dest.GetUnsafe<float>();
				break;
			}
		}
		else
		{
//			v = Vec3V(V_ZERO);
		}

		m_Index++;
	}

	atArray<Vec3V>* m_WidgetValues;
	atArray<bool>* m_WidgetInvalid;
	int m_Index;
};

void FrameEditorWidgets::ResetWidgets()
{
	Assert(m_Frame);

	ResetWidgetIterator it(*m_Frame, m_WidgetValues, m_WidgetInvalid);
	it.Iterate(NULL, 1.f, false);
}

class UpdateWidgetIterator : public crFrameIterator<UpdateWidgetIterator>
{
public:
	UpdateWidgetIterator(crFrame& frame, atArray<Vec3V>& widgetValues, atArray<bool>& widgetInvalid)
		: crFrameIterator<UpdateWidgetIterator>(frame)
		, m_WidgetValues(&widgetValues)
		, m_WidgetInvalid(&widgetInvalid)
	{
		m_Index = 0;
	}

	void IterateDof(const crFrameData::Dof& dof, crFrame::Dof& dest, float)
	{
		const Vec3V& v = (*m_WidgetValues)[m_Index];

		switch(dof.m_Type)
		{
		case kFormatTypeQuaternion:
			{
				QuatV q = QuatVFromEulersXYZ(v);
				dest.Set<QuatV>(q);
			}
			break;

		case kFormatTypeVector3:
			dest.Set<Vec3V>(v);
			break;

		case kFormatTypeFloat:
			dest.Set<float>(v[0]);
			break;
		}

		const bool& b = (*m_WidgetInvalid)[m_Index];
		dest.SetInvalid(b);

		m_Index++;
	}

	atArray<Vec3V>* m_WidgetValues;
	atArray<bool>* m_WidgetInvalid;
	int m_Index;
};

void FrameEditorWidgets::UpdateFrame()
{
	Assert(m_Frame);

	UpdateWidgetIterator it(*m_Frame, m_WidgetValues, m_WidgetInvalid);
	it.Iterate(NULL, 1.f, false);
}

CEntityCapture::~CEntityCapture()
{
	if(m_pGroup)
	{
		m_pGroup->Destroy(); m_pGroup = NULL;
	}

	m_GroupName.Reset();

	m_pBank = NULL;
	m_pEntity = NULL;
}

CEntityCapture::CEntityCapture(CDynamicEntity *pEntity, bkBank *pBank, bkGroup *pParentGroup)
	: m_pEntity(pEntity)
	, m_pBank(pBank)
	, m_pParentGroup(pParentGroup)
	, m_pGroup(NULL)
	, m_iCaptureFilterNameIndex(0)
	, m_pAuthoringHelper(NULL)
	, m_bStarted(false)
	, m_bShowOrigin(false)
{
	if(animVerifyf(m_pEntity, "") && animVerifyf(m_pBank, "") && animVerifyf(m_pParentGroup, ""))
	{
		m_GroupName = atVarString("%s (%p)", m_pEntity->GetModelName(), m_pEntity.Get());

		strcpy(m_szAnimationName, m_pEntity->GetModelName());
		strcpy(m_szCompressionFile, "MG_PlayerMovement_HighQuality.txt");
		strcpy(m_szSkelFile, "biped.skel");

		m_InitialMover = m_pEntity->GetMatrix();
		m_InitialMoverPosition = m_InitialMover.d();
		Vec3V eulers = QuatVToEulersXYZ(QuatVFromMat34V(m_InitialMover));
		m_InitialMoverRotation[0] = eulers.GetXf() * RtoD;
		m_InitialMoverRotation[1] = eulers.GetYf() * RtoD;
		m_InitialMoverRotation[2] = eulers.GetZf() * RtoD;

		m_pBank->SetCurrentGroup(*m_pParentGroup);
		{
			m_pGroup = m_pBank->PushGroup(m_GroupName.c_str());
			{
				m_pBank->AddText("Animation name:", m_szAnimationName, 256);
				m_pBank->AddButton("Set Ped Defaults", datCallback(MFA(CEntityCapture::SetPedDefaults), (datBase*)this));
				m_pBank->AddButton("Set Horse Defaults", datCallback(MFA(CEntityCapture::SetHorseDefaults), (datBase*)this));
				m_pBank->AddText("Compression file:", m_szCompressionFile, 256);
				m_pBank->AddText("Skel file:", m_szSkelFile, 256);
				m_pBank->AddCombo("Filter", &m_iCaptureFilterNameIndex, CAnimViewer::m_iCaptureFilterNameCount, CAnimViewer::m_szCaptureFilterNames);
				m_pBank->AddButton("Show / Hide Origin", datCallback(MFA(CEntityCapture::ShowHideOrigin), (datBase*)this));
				m_pBank->AddButton("Reset Origin", datCallback(MFA(CEntityCapture::ResetOrigin), (datBase*)this));
				m_pBank->AddVector("Origin Position", &m_InitialMoverPosition, -100000.0f, 100000.0f, 0.001f, datCallback(MFA(CEntityCapture::InitialMoverChanged), (datBase*)this));
				m_pBank->AddAngle("Origin Rotation X", &m_InitialMoverRotation[0], bkAngleType::DEGREES, datCallback(MFA(CEntityCapture::InitialMoverChanged), (datBase*)this));
				m_pBank->AddAngle("Origin Rotation Y", &m_InitialMoverRotation[1], bkAngleType::DEGREES, datCallback(MFA(CEntityCapture::InitialMoverChanged), (datBase*)this));
				m_pBank->AddAngle("Origin Rotation Z", &m_InitialMoverRotation[2], bkAngleType::DEGREES, datCallback(MFA(CEntityCapture::InitialMoverChanged), (datBase*)this));
				m_pGroup->AddButton("Remove Entity", datCallback(MFA(CEntityCapture::RemoveEntity), (datBase*)this));
			}
			m_pBank->PopGroup();
		}
		m_pBank->UnSetCurrentGroup(*m_pParentGroup);
	}
}

void CEntityCapture::SetPedDefaults()
{
	for(int i = 0; i < CAnimViewer::m_iCaptureFilterNameCount; i ++)
	{
		if(stricmp(CAnimViewer::m_szCaptureFilterNames[i], "(null filter)") == 0)
		{
			m_iCaptureFilterNameIndex = i;

			break;
		}
	}
	strcpy(m_szCompressionFile, "MG_PlayerMovement_HighQuality.txt");
	strcpy(m_szSkelFile, "biped.skel");
}

void CEntityCapture::SetHorseDefaults()
{
	for(int i = 0; i < CAnimViewer::m_iCaptureFilterNameCount; i ++)
	{
		if(stricmp(CAnimViewer::m_szCaptureFilterNames[i], "(null filter)") == 0)
		{
			m_iCaptureFilterNameIndex = i;

			break;
		}
	}
	strcpy(m_szCompressionFile, "MG_Horse_HighQuality.txt");
	strcpy(m_szSkelFile, "horse.skel");
}

void CEntityCapture::ShowHideOrigin()
{
	m_bShowOrigin = !m_bShowOrigin;

	if(m_bShowOrigin)
	{
		if(animVerifyf(!m_pAuthoringHelper, ""))
		{
			m_pAuthoringHelper = new CAuthoringHelper("Entity Capture Origin");
			m_pAuthoringHelper->Update(RC_MATRIX34(m_InitialMover));
		}
	}
	else
	{
		if(animVerifyf(m_pAuthoringHelper, ""))
		{
			delete m_pAuthoringHelper; m_pAuthoringHelper = NULL;
		}
	}
}

void CEntityCapture::ResetOrigin()
{
	if(m_pEntity)
	{
		m_InitialMover = m_pEntity->GetMatrix();
		m_InitialMoverPosition = m_InitialMover.d();
		Vec3V eulers = QuatVToEulersXYZ(QuatVFromMat34V(m_InitialMover));
		m_InitialMoverRotation[0] = eulers.GetXf() * RtoD;
		m_InitialMoverRotation[1] = eulers.GetYf() * RtoD;
		m_InitialMoverRotation[2] = eulers.GetZf() * RtoD;
	}
}

void CEntityCapture::InitialMoverChanged()
{
	if(m_pAuthoringHelper)
	{
		Vec3V eulers;
		eulers.SetXf(m_InitialMoverRotation[0] * DtoR);
		eulers.SetYf(m_InitialMoverRotation[1] * DtoR);
		eulers.SetZf(m_InitialMoverRotation[2] * DtoR);
		Mat34VFromEulersXYZ(m_InitialMover, eulers, m_InitialMoverPosition);
	}
}

void CEntityCapture::Update()
{
	if (m_bShowOrigin && m_pAuthoringHelper)
	{
		m_pAuthoringHelper->Update(RC_MATRIX34(m_InitialMover));
		m_InitialMoverPosition = m_InitialMover.d();
		Vec3V eulers = QuatVToEulersXYZ(QuatVFromMat34V(m_InitialMover));
		m_InitialMoverRotation[0] = eulers.GetXf() * RtoD;
		m_InitialMoverRotation[1] = eulers.GetYf() * RtoD;
		m_InitialMoverRotation[2] = eulers.GetZf() * RtoD;
	}
}

void CEntityCapture::Start()
{
	animDisplayf("Capturing entity...\n");

	if(animVerifyf(!m_bStarted, "Entity capture already started!"))
	{
		if(animVerifyf(strlen(m_szAnimationName) > 0, "Entity capture name must not be empty!"))
		{
			if(animVerifyf(strchr(m_szAnimationName, '.') == NULL, "Entity capture name should not have an extension, these will be added automatically!"))
			{
				if(animVerifyf(m_pEntity, "Entity capture needs a selected entity!"))
				{
					// Start entity capture
					m_bStarted = true;

					// Make initial mover upright so that we capture any movement on slopes for entities that aren't constrained upright
					Matrix34 initalMover = MAT34V_TO_MATRIX34(m_InitialMover);
					initalMover.MakeUpright();
					m_InitialMover = MATRIX34_TO_MAT34V(initalMover);
					m_InitialMoverPosition = m_InitialMover.d();
					Vec3V eulers = QuatVToEulersXYZ(QuatVFromMat34V(m_InitialMover));
					m_InitialMoverRotation[0] = eulers.GetXf() * RtoD;
					m_InitialMoverRotation[1] = eulers.GetYf() * RtoD;
					m_InitialMoverRotation[2] = eulers.GetZf() * RtoD;
				}
			}
		}
	}
}

void CEntityCapture::RenderUpdate()
{
	if(m_bStarted)
	{
		if(fwTimer::GetSingleStepThisFrame() || (!fwTimer::IsUserPaused() && !fwTimer::IsScriptPaused() && !fwTimer::IsGamePaused()))
		{
			if(animVerifyf(m_bStarted, "Entity capture has not been started!"))
			{
				if(animVerifyf(strlen(m_szAnimationName) > 0, "Entity capture name must not be empty!"))
				{
					if(animVerifyf(strchr(m_szAnimationName, '.') == NULL, "Entity capture name should not have an extension, these will be added automatically!"))
					{
						if(animVerifyf(m_pEntity, "Entity capture needs a selected entity!"))
						{
							// Allocate frame
							crFrame *pFrame = rage_new crFrame;
							if(animVerifyf(pFrame, "Entity capture could not allocate frame"))
							{
								// Get skeleton / skeleton data
								const crSkeletonData &skelData = m_pEntity->GetSkeletonData();
								crSkeleton &skel = *m_pEntity->GetSkeleton();

								// Capture frame
								crFrameFilter *pFilter = NULL;
								if(m_iCaptureFilterNameIndex >= 0 && m_iCaptureFilterNameIndex < CAnimViewer::m_iCaptureFilterNameCount)
								{
									pFilter = g_FrameFilterDictionaryStore.FindFrameFilter(fwMvFilterId(CAnimViewer::m_szCaptureFilterNames[m_iCaptureFilterNameIndex]));
								}
								pFrame->InitCreateBoneAndMoverDofs(skelData, true, 0, true, pFilter);
								pFrame->IdentityFromSkel(skelData);
								pFrame->InversePose(skel);

								Mat34V moverMatrix = m_pEntity->GetMatrix();

								UnTransformOrtho(moverMatrix, m_InitialMover, moverMatrix);

								pFrame->SetVector3(kTrackMoverTranslation, 0, moverMatrix.GetCol3());

								QuatV moverRotation = QuatVFromMat34V(moverMatrix);
								pFrame->SetQuaternion(kTrackMoverRotation, 0, moverRotation);

								// Add frame to array
								m_FrameArray.PushAndGrow(pFrame, 1);

								animDisplayf("Captured entity frame %i...\n", m_FrameArray.GetCount());
							}
						}
					}
				}
			}
		}
	}
}

void CEntityCapture::Stop()
{
	if(animVerifyf(m_bStarted, "Entity capture has not been started!"))
	{
		if(animVerifyf(strlen(m_szAnimationName) > 0, "Entity capture name must not be empty!"))
		{
			if(animVerifyf(strchr(m_szAnimationName, '.') == NULL, "Entity capture name should not have an extension, these will be added automatically!"))
			{
				// Check we have at least one frame to save
				if(animVerifyf(m_FrameArray.GetCount() > 1, "Entity capture needs at least two frames to save!"))
				{
					// Stop entity capture
					m_bStarted = false;
					m_InitialMover = Mat34V(V_IDENTITY);
					m_InitialMoverPosition = m_InitialMover.d();
					Vec3V eulers = QuatVToEulersXYZ(QuatVFromMat34V(m_InitialMover));
					m_InitialMoverRotation[0] = eulers.GetXf() * RtoD;
					m_InitialMoverRotation[1] = eulers.GetYf() * RtoD;
					m_InitialMoverRotation[2] = eulers.GetZf() * RtoD;

					// Calculate duration
					float duration = (m_FrameArray.GetCount() - 1) * (1.0f / 30.0f);

					bool mover = false;
					for(int i = 0; i < m_FrameArray.GetCount(); i ++)
					{
						const crFrame *pFrame = m_FrameArray[i];
						if(pFrame->HasDof(kTrackMoverTranslation, 0) || pFrame->HasDof(kTrackMoverRotation, 0))
						{
							mover = true;

							break;
						}
					}

					// Allocate animation
					crAnimation *pAnimation = rage_new crAnimation;
					if(animVerifyf(pAnimation, "Entity capture could not allocate animation!"))
					{
						// Create animation
						if(animVerifyf(pAnimation->Create(m_FrameArray.GetCount(), duration, false, mover), "Entity capture could not create animation!"))
						{
							pAnimation->CreateFromFrames(m_FrameArray, LOSSLESS_ANIM_TOLERANCE);
							pAnimation->SetName(m_szAnimationName);

							ASSET.PushFolder("X:/gta5");

							// Save animation
							animDisplayf("Saving animation '%s'...\n", atVarString("%s.anim", m_szAnimationName).c_str());
							if(animVerifyf(pAnimation->Save(atVarString("%s.anim", m_szAnimationName).c_str()), "Entity capture could not save animation!"))
							{
								animDisplayf("done.\n");

								// Allocate clip
								crClipAnimation *pClip = rage_new crClipAnimation;
								if(animVerifyf(pClip, "Entity capture could not allocate clip!"))
								{
									// Create clip
									if(animVerifyf(pClip->Create(*pAnimation), "Entity capture could not create clip!"))
									{
										if(mover)
										{
											const crFrame *pFrame = m_FrameArray[0];

											/* Initial Offset */

											Vec3V initialOffsetPositionVec3V;
											pFrame->GetVector3(kTrackMoverTranslation, 0, initialOffsetPositionVec3V);

											crPropertyAttributeVector3 initialOffsetPositionAttrib;
											initialOffsetPositionAttrib.SetName("InitialOffsetPosition");
											initialOffsetPositionAttrib.GetVector3() = initialOffsetPositionVec3V;

											crProperty initialOffsetPositionProp;
											initialOffsetPositionProp.SetName("InitialOffsetPosition");
											initialOffsetPositionProp.AddAttribute(initialOffsetPositionAttrib);

											/* Initial Rotation */

											QuatV initialOffsetRotationQuatV;
											pFrame->GetQuaternion(kTrackMoverRotation, 0, initialOffsetRotationQuatV);

											crPropertyAttributeQuaternion initialOffsetRotationAttrib;
											initialOffsetRotationAttrib.SetName("InitialOffsetRotation");
											initialOffsetRotationAttrib.GetQuaternion() = initialOffsetRotationQuatV;

											crProperty initialOffsetRotationProp;
											initialOffsetRotationProp.SetName("InitialOffsetRotation");
											initialOffsetRotationProp.AddAttribute(initialOffsetRotationAttrib);

											crProperties *pProperties = pClip->GetProperties();
											pProperties->AddProperty(initialOffsetPositionProp);
											pProperties->AddProperty(initialOffsetRotationProp);
										}

										/* Compression File */

										crPropertyAttributeString compressionFileAttrib;
										compressionFileAttrib.SetName("Compressionfile_DO_NOT_RESOURCE");
										compressionFileAttrib.GetString() = m_szCompressionFile;

										crProperty compressionFileProp;
										compressionFileProp.SetName("Compressionfile_DO_NOT_RESOURCE");
										compressionFileProp.AddAttribute(compressionFileAttrib);

										/* Skel File */

										crPropertyAttributeString skelFileAttrib;
										skelFileAttrib.SetName("Skelfile_DO_NOT_RESOURCE");
										skelFileAttrib.GetString() = m_szSkelFile;

										crProperty skelFileProp;
										skelFileProp.SetName("Skelfile_DO_NOT_RESOURCE");
										skelFileProp.AddAttribute(skelFileAttrib);

										crProperties *pProperties = pClip->GetProperties();
										pProperties->AddProperty(compressionFileProp);
										pProperties->AddProperty(skelFileProp);

										// Save clip
										animDisplayf("Saving clip '%s'...\n", atVarString("%s.clip", m_szAnimationName).c_str());
										animVerifyf(pClip->Save(atVarString("%s.clip", m_szAnimationName).c_str()), "Entity capture could not save clip!");
										animDisplayf("done.\n");
									}

									// Release clip
									delete pClip; pClip = NULL;
								}
							}

							ASSET.PopFolder();
						}

						// Release animation
						delete pAnimation; pAnimation = NULL;
					}

					// Release frames
					for(int i = 0; i < m_FrameArray.GetCount(); i ++)
					{
						// Release frame
						delete m_FrameArray[i]; m_FrameArray[i] = NULL;
					}
					m_FrameArray.Reset();
				}
			}
		}
	}
}

void CEntityCapture::SingleFrame()
{
	animDisplayf("Capturing single entity frame...\n");

	if(animVerifyf(strlen(m_szAnimationName) > 0, "Entity capture name must not be empty!"))
	{
		if(animVerifyf(strchr(m_szAnimationName, '.') == NULL, "Entity capture name should not have an extension, these will be added automatically!"))
		{
			if(animVerifyf(m_pEntity, "Entity capture needs a selected entity!"))
			{
				if(animVerifyf(!m_bStarted, "Entity capture is already started!"))
				{
					// Start entity capture
					m_InitialMover = m_pEntity->GetMatrix();
					m_InitialMoverPosition = m_InitialMover.d();
					Vec3V eulers = QuatVToEulersXYZ(QuatVFromMat34V(m_InitialMover));
					m_InitialMoverRotation[0] = eulers.GetXf() * RtoD;
					m_InitialMoverRotation[1] = eulers.GetYf() * RtoD;
					m_InitialMoverRotation[2] = eulers.GetZf() * RtoD;

					// Allocate frame
					crFrame *pFrame = rage_new crFrame;
					if(animVerifyf(pFrame, "Entity capture could not allocate frame"))
					{
						// Get skeleton / skeleton data
						const crSkeletonData &skelData = m_pEntity->GetSkeletonData();
						crSkeleton &skel = *m_pEntity->GetSkeleton();

						// Capture frame
						crFrameFilter *pFilter = NULL;
						if(m_iCaptureFilterNameIndex >= 0 && m_iCaptureFilterNameIndex < CAnimViewer::m_iCaptureFilterNameCount)
						{
							pFilter = g_FrameFilterDictionaryStore.FindFrameFilter(fwMvFilterId(CAnimViewer::m_szCaptureFilterNames[m_iCaptureFilterNameIndex]));
						}
						pFrame->InitCreateBoneAndMoverDofs(skelData, true, 0, true, pFilter);
						pFrame->IdentityFromSkel(skelData);
						pFrame->InversePose(skel);

						Mat34V moverMatrix = m_pEntity->GetMatrix();
						UnTransformOrtho(moverMatrix, m_InitialMover, moverMatrix);
						pFrame->SetVector3(kTrackMoverTranslation, 0, moverMatrix.GetCol3());

						QuatV moverRotation = QuatVFromMat34V(moverMatrix);
						pFrame->SetQuaternion(kTrackMoverRotation, 0, moverRotation);

						// Add frame to array
						m_FrameArray.PushAndGrow(pFrame, 1);

						// Add frame to array
						m_FrameArray.PushAndGrow(pFrame, 1);
					}

					// Stop entity capture
					m_InitialMover = Mat34V(V_IDENTITY);
					m_InitialMoverPosition = m_InitialMover.d();
					eulers = QuatVToEulersXYZ(QuatVFromMat34V(m_InitialMover));
					m_InitialMoverRotation[0] = eulers.GetXf() * RtoD;
					m_InitialMoverRotation[1] = eulers.GetYf() * RtoD;
					m_InitialMoverRotation[2] = eulers.GetZf() * RtoD;

					// Calculate duration
					float duration = (m_FrameArray.GetCount() - 1) * (1.0f / 30.0f);

					bool mover = false;
					for(int i = 0; i < m_FrameArray.GetCount(); i ++)
					{
						const crFrame *pFrame = m_FrameArray[i];
						if(pFrame->HasDof(kTrackMoverTranslation, 0) || pFrame->HasDof(kTrackMoverRotation, 0))
						{
							mover = true;

							break;
						}
					}

					// Allocate animation
					crAnimation *pAnimation = rage_new crAnimation;
					if(animVerifyf(pAnimation, "Entity capture could not allocate animation!"))
					{
						// Create animation
						if(animVerifyf(pAnimation->Create(m_FrameArray.GetCount(), duration), "Entity capture could not create animation!"))
						{
							pAnimation->CreateFromFrames(m_FrameArray, LOSSLESS_ANIM_TOLERANCE);
							pAnimation->SetName(m_szAnimationName);

							ASSET.PushFolder("X:/gta5");

							// Save animation
							animDisplayf("Saving animation '%s'\n...", atVarString("%s.anim", m_szAnimationName).c_str());
							if(animVerifyf(pAnimation->Save(atVarString("%s.anim", m_szAnimationName).c_str()), "Entity capture could not save animation!"))
							{
								animDisplayf("done.\n");

								// Allocate clip
								crClipAnimation *pClip = rage_new crClipAnimation;
								if(animVerifyf(pClip, "Entity capture could not allocate clip!"))
								{
									// Create clip
									if(animVerifyf(pClip->Create(*pAnimation), "Entity capture could not create clip!"))
									{
										if(mover)
										{
											const crFrame *pFrame = m_FrameArray[0];

											/* Initial Offset */

											Vec3V initialOffsetPositionVec3V;
											pFrame->GetVector3(kTrackMoverTranslation, 0, initialOffsetPositionVec3V);

											crPropertyAttributeVector3 initialOffsetPositionAttrib;
											initialOffsetPositionAttrib.SetName("InitialOffsetPosition");
											initialOffsetPositionAttrib.GetVector3() = initialOffsetPositionVec3V;

											crProperty initialOffsetPositionProp;
											initialOffsetPositionProp.SetName("InitialOffsetPosition");
											initialOffsetPositionProp.AddAttribute(initialOffsetPositionAttrib);

											/* Initial Rotation */

											QuatV initialOffsetRotationQuatV;
											pFrame->GetQuaternion(kTrackMoverRotation, 0, initialOffsetRotationQuatV);

											crPropertyAttributeQuaternion initialOffsetRotationAttrib;
											initialOffsetRotationAttrib.SetName("InitialOffsetRotation");
											initialOffsetRotationAttrib.GetQuaternion() = initialOffsetRotationQuatV;

											crProperty initialOffsetRotationProp;
											initialOffsetRotationProp.SetName("InitialOffsetRotation");
											initialOffsetRotationProp.AddAttribute(initialOffsetRotationAttrib);

											crProperties *pProperties = pClip->GetProperties();
											pProperties->AddProperty(initialOffsetPositionProp);
											pProperties->AddProperty(initialOffsetRotationProp);
										}

										/* Compression File */

										crPropertyAttributeString compressionFileAttrib;
										compressionFileAttrib.SetName("Compressionfile_DO_NOT_RESOURCE");
										compressionFileAttrib.GetString() = m_szCompressionFile;

										crProperty compressionFileProp;
										compressionFileProp.SetName("Compressionfile_DO_NOT_RESOURCE");
										compressionFileProp.AddAttribute(compressionFileAttrib);

										/* Skel File */

										crPropertyAttributeString skelFileAttrib;
										skelFileAttrib.SetName("Skelfile_DO_NOT_RESOURCE");
										skelFileAttrib.GetString() = m_szSkelFile;

										crProperty skelFileProp;
										skelFileProp.SetName("Skelfile_DO_NOT_RESOURCE");
										skelFileProp.AddAttribute(skelFileAttrib);

										crProperties *pProperties = pClip->GetProperties();
										pProperties->AddProperty(compressionFileProp);
										pProperties->AddProperty(skelFileProp);

										// Save clip
										animDisplayf("Saving clip '%s'\n...", atVarString("%s.clip", m_szAnimationName).c_str());
										animVerifyf(pClip->Save(atVarString("%s.clip", m_szAnimationName).c_str()), "Entity capture could not save clip!");
										animDisplayf("done.\n");
									}

									// Release clip
									delete pClip; pClip = NULL;
								}
							}

							ASSET.PopFolder();
						}

						// Release animation
						delete pAnimation; pAnimation = NULL;
					}

					// Release frame
					delete m_FrameArray[0]; m_FrameArray[0] = NULL;

					// Release frames array
					m_FrameArray.Reset();
				}
			}
		}
	}
}

void CEntityCapture::RemoveEntity()
{
	if(m_bStarted)
	{
		Stop();
	}

	if(m_pAuthoringHelper)
	{
		delete m_pAuthoringHelper; m_pAuthoringHelper = NULL;
	}

	if(m_pGroup)
	{
		m_pGroup->Destroy();

		m_pBank = NULL;
		m_pParentGroup = NULL;
		m_pGroup = NULL;
		m_pEntity = NULL;
	}

	for(int i = 0; i < CAnimViewer::m_EntityCaptureArray.GetCount(); i ++)
	{
		if(CAnimViewer::m_EntityCaptureArray[i] == this)
		{
			CAnimViewer::m_EntityCaptureArray.Delete(i);

			break;
		}
	}
}

void CAnimViewer::sDebugExpressions::AddExpressionDebugWidgets(CDynamicEntity& dynamicEntity, bkGroup& bk)
{
	m_pDynamicEntity = &dynamicEntity;

#if DEBUG_EXPRESSIONS
	if(m_pDynamicEntity->GetAnimDirector())
	{
		fwAnimDirector& director = *m_pDynamicEntity->GetAnimDirector();
		fwAnimDirectorComponentExpressions* componentExpressions = director.GetComponentByPhase<fwAnimDirectorComponentExpressions>(m_Phase);

		bk.AddButton("Reset Override", datCallback(MFA(CAnimViewer::sDebugExpressions::ResetExpressionDebugWidgets), (datBase*)this));

		if (componentExpressions)
		{
			bkGroup* bkExpressionsGroup = bk.AddGroup("Expressions");
			m_Weights.Resize(componentExpressions->GetDebugObservers().GetCount());
			for(int i=0; i<componentExpressions->GetDebugObservers().GetCount(); ++i)
			{
				const char* exprName = "UNKNOWN";

				m_Weights[i] = 0.f;
				if(componentExpressions->GetDebugObservers()[i] && componentExpressions->GetDebugObservers()[i]->GetNodeSafe())
				{
					crmtNodeExpression& nodeExpr = *componentExpressions->GetDebugObservers()[i]->GetNodeSafe();
					m_Weights[i] = nodeExpr.GetWeight();

					if(nodeExpr.GetExpressionPlayer().GetExpressions())
					{
						exprName = nodeExpr.GetExpressionPlayer().GetExpressions()->GetName();
					}
				}

				bkExpressionsGroup->AddSlider(exprName, &m_Weights[i], 0.f, 1.f, 0.001f);

				// attempt to load a matching .hashnames file for this expression
				CBaseModelInfo* pMI = CModelInfo::GetBaseModelInfo(m_pDynamicEntity->GetModelId());
				animAssert(pMI);
				if (pMI)
				{
					strLocalIndex iExpressionDictionaryIndex = strLocalIndex(pMI->GetExpressionDictionaryIndex());
					fwExpressionsDictionaryLoader loader(iExpressionDictionaryIndex.Get(), false);
					if(loader.IsLoaded())
					{
						bool loadedHashNames = false;
						for(int tryComponent=0; tryComponent<=1; tryComponent++)
						{
							atString prefix;
							prefix = "assets:/anim/expressions/";
							prefix += g_ExpressionDictionaryStore.GetName(iExpressionDictionaryIndex);

							if(tryComponent)
							{
								prefix += "/components";
							}

							atString hashFilename(exprName);
							hashFilename.Replace("pack:", prefix.c_str());
							hashFilename += ".hashnames";

							if(LoadHashNames(hashFilename.c_str()))
							{
								loadedHashNames = true;
								break;
							}
						}
						if(!loadedHashNames)
						{
							Displayf("CAnimViewer::sDebugExpressions::LoadHashNames - failed to open hash file for '%s/%s'", g_ExpressionDictionaryStore.GetName(iExpressionDictionaryIndex), exprName);
						}
					}
				}
			}

			crFrame& frameIn = componentExpressions->GetDebugFrameIn();
			bkGroup* bkInputGroup = bk.AddGroup("Input frame");
			m_FrameInWidgets.Init(frameIn, &m_pDynamicEntity->GetSkeletonData(), true);
			m_FrameInWidgets.AddWidgets(*bkInputGroup);

			crFrame& frameOut = componentExpressions->GetDebugFrameOut();
			bkGroup* bkOutputGroup = bk.AddGroup("Output frame");
			m_FrameOutWidgets.Init(frameOut, &m_pDynamicEntity->GetSkeletonData(), true);
			m_FrameOutWidgets.AddWidgets(*bkOutputGroup);

			crFrame& frameOverride = componentExpressions->GetDebugFrameOverride();
			bkGroup* bkOverrideGroup = bk.AddGroup("Override frame");
			m_FrameOverrideWidgets.Init(frameOverride, &m_pDynamicEntity->GetSkeletonData(), false);
			m_FrameOverrideWidgets.AddWidgets(*bkOverrideGroup);
		}

		ResetExpressionDebugWidgets();
	}
#endif // DEBUG_EXPRESSIONS
}

void CAnimViewer::sDebugExpressions::RemoveExpressionDebugWidgets()
{
#if DEBUG_EXPRESSIONS
	if(m_pExpressionWidgetGroup && m_pExpressionParentGroup)
	{
		m_pExpressionParentGroup->Remove(*m_pExpressionWidgetGroup);
		m_pExpressionWidgetGroup = NULL;
	}

	m_FrameInWidgets.Shutdown();
	m_FrameOutWidgets.Shutdown();
	m_FrameOverrideWidgets.Shutdown();

	m_Weights.Reset();

	m_pDynamicEntity = NULL;
#endif // DEBUG_EXPRESSIONS
}

void CAnimViewer::sDebugExpressions::UpdateExpressionDebugWidgets()
{
#if DEBUG_EXPRESSIONS
	if(m_pDynamicEntity)
	{
		fwAnimDirector& director = *m_pDynamicEntity->GetAnimDirector();
		fwAnimDirectorComponentExpressions* componentExpressions = director.GetComponentByPhase<fwAnimDirectorComponentExpressions>(m_Phase);
		if (componentExpressions)
		{
			m_FrameOverrideWidgets.UpdateFrame();

			m_FrameInWidgets.ResetWidgets();
			m_FrameOutWidgets.ResetWidgets();

			if(m_pDynamicEntity->GetAnimDirector())
			{
				atArray<crmtObserverTyped<crmtNodeExpression>* >& observers = componentExpressions->GetDebugObservers();
				for(int i=0; i<Min(observers.GetCount(), m_Weights.GetCount()); ++i)
				{
					if(observers[i] && observers[i]->GetNodeSafe())
					{
						observers[i]->GetNodeSafe()->SetWeight(m_Weights[i]);
					}
				}
			}
		}
	}
#endif // DEBUG_EXPRESSIONS
}

void CAnimViewer::sDebugExpressions::ResetExpressionDebugWidgets()
{
#if DEBUG_EXPRESSIONS
	if (m_pDynamicEntity && m_pDynamicEntity->GetAnimDirector())
	{
		fwAnimDirector& director = *m_pDynamicEntity->GetAnimDirector();
		fwAnimDirectorComponentExpressions* componentExpressions = director.GetComponentByPhase<fwAnimDirectorComponentExpressions>(m_Phase);
		if (componentExpressions)
		{
			crFrame& frameOverride = componentExpressions->GetDebugFrameOverride();
			crCreature* creature = m_pDynamicEntity->GetCreature();
			if (creature)
			{
				creature->Identity(frameOverride);
			}
			else
			{
				frameOverride.IdentityFromSkel(m_pDynamicEntity->GetSkeletonData());
			}

			m_FrameOverrideWidgets.ResetWidgets();
			frameOverride.Invalidate();
			m_FrameOverrideWidgets.ResetWidgets();
		}
	}
#endif // DEBUG_EXPRESSIONS
}

#if DEBUG_EXPRESSIONS
bool CAnimViewer::sDebugExpressions::LoadHashNames(const char* hashFilename)
{
	if(ASSET.Exists(hashFilename, ""))
	{
		fiSafeStream f(reinterpret_cast<fiStream*>(CFileMgr::OpenFile(hashFilename)));
		if(f)
		{
			fiTokenizer T(hashFilename, f);	

			const int maxBufSize = 256;
			char buf[maxBufSize];
			while(T.GetLine(buf, maxBufSize) > 0)
			{
				char* ch = buf;

				const char* trackIdxString = ch;
				while(*ch)
				{
					if(*ch == ',')
					{
						*ch = '\0';
						ch++;
						break;
					}
					ch++;
				}
				const char* dofIdString = ch;
				while(*ch)
				{
					if(*ch == ',')
					{
						*ch = '\0';
						ch++;
						break;
					}
					ch++;
				}
				const char* nameString = ch;

				if(*trackIdxString && *dofIdString && *nameString)
				{
	//				u8 trackIdx = u8(atoi(trackIdxString));
					u16 dofId = u16(atoi(dofIdString));

					m_pDynamicEntity->GetSkeletonData().DebugRegisterBoneIdToName(dofId, nameString);
				}
			}

			return true;
		}
	}
	return false;
}
#else // DEBUG_EXPRESSIONS
bool CAnimViewer::sDebugExpressions::LoadHashNames(const char*)
{
	return false;
}
#endif // DEBUG_EXPRESSIONS

#if __DEV

void CAnimViewer::DumpClipSetsCSV()
{
	char szBuffer[256];

	// Open CSV file
	fiStream *csvFile = fiStream::Create("ClipSets.csv");

	// Write header
	sprintf(szBuffer, "Clip Set,Fallback Set,Clip Dictionary,Streaming Policy,Memory Group,Clip,Clip Flags,Priority,Bone Mask\n");
	csvFile->WriteByte(szBuffer, istrlen(szBuffer));

	// Iterate through clip sets
	for(int clipSetIndex = 0; clipSetIndex < fwClipSetManager::GetClipSetCount(); clipSetIndex ++)
	{
		fwMvClipSetId clipSetId = fwClipSetManager::GetClipSetIdByIndex(clipSetIndex);
		const fwClipSet *clipSet = fwClipSetManager::GetClipSetByIndex(clipSetIndex);

		// Iterate through clip items
		for(int clipItemIndex = 0; clipItemIndex < clipSet->GetClipItemCount(); clipItemIndex ++)
		{
			fwMvClipId clipId = clipSet->GetClipItemIdByIndex(clipItemIndex);
			const fwClipItem *clipItem = clipSet->GetClipItemByIndex(clipItemIndex);

			atString streamingPolicy;
			eStreamingPolicy_ToString((eStreamingPolicy)clipSet->GetClipDictionaryStreamingPolicy(), streamingPolicy);

			atHashString memoryGroup(atHashString::Null());
			if (clipSet->GetClipDictionaryMetadata())
			{
				memoryGroup = clipSet->GetClipDictionaryMetadata()->GetMemoryGroup();
			}

			// Write row
			sprintf(szBuffer, "%s,%s,%s,%s,%s,%s,%u,%s,%s\n",
				clipSetId.GetCStr(),
				clipSet->GetFallbackId().GetCStr() ? clipSet->GetFallbackId().GetCStr() : "",
				clipSet->GetClipDictionaryName().GetCStr(),
				streamingPolicy.c_str(),
				memoryGroup.GetCStr(),
				clipId.GetCStr(),
				GetClipItemFlags(clipItem),
				eAnimPriority_ToString(GetClipItemPriority(clipItem)),
				GetClipItemBoneMask(clipItem).GetCStr());
			csvFile->WriteByte(szBuffer, istrlen(szBuffer));
		}
	}

	// Close CSV file
	csvFile->Close();
}

void CAnimViewer::DumpFiltersCSV()
{
	ASSET.PushFolder("assets:/anim/filters");

	char szBuffer[256];

	// Open CSV file
	fiStream *csvFile = ASSET.Create("Filters", "csv");

	// Write header
	sprintf(szBuffer, "Filter Dictionary Index,Filter Dictionary Name,Filter Index,Filter Name,DoF Index,DoF Track,DoF Id,DoF Weight Group Index,DoF Weight\n");
	csvFile->WriteByte(szBuffer, istrlen(szBuffer));

	// Iterate through filter dictionaries
	for(int i = 0; i < g_FrameFilterDictionaryStore.GetCount(); i ++)
	{
		if(g_FrameFilterDictionaryStore.IsValidSlot(strLocalIndex(i)) && g_FrameFilterDictionaryStore.IsObjectInImage(strLocalIndex(i)))
		{
			// Get filter dictionary
			crFrameFilterDictionary *frameFilterDictionary = g_FrameFilterDictionaryStore.Get(strLocalIndex(i));
			const char *frameFilterDictionaryName = g_FrameFilterDictionaryStore.GetName(strLocalIndex(i));
			if(frameFilterDictionary && frameFilterDictionaryName)
			{
				// Iterate through filters
				for(int j = 0; j < frameFilterDictionary->GetCount(); j ++)
				{
					crFrameFilter *frameFilter = frameFilterDictionary->GetEntry(j);
					const char *frameFilterName = atHashString(frameFilterDictionary->GetCode(j)).TryGetCStr();
					if(frameFilter && frameFilterName)
					{
						crFrameFilterMultiWeight *frameFilterMultiWeight = frameFilter->DynamicCast<crFrameFilterMultiWeight>();
						if(frameFilterMultiWeight)
						{
							// Iterate through DoFs
							for(int k = 0; k < frameFilterMultiWeight->GetEntryCount(); k ++)
							{
								// Get DoF
								u8 track;
								u16 id;
								int index;
								float weight;
								frameFilterMultiWeight->GetEntry(k, track, id, index, weight);

								const char* trackName = crAnimTrack::ConvertTrackIndexToName(track);
								char trackId[16];
								if(!trackName)
								{
									sprintf(trackId, "%u", (unsigned int)track);
									trackName = trackId;
								}

								const char* boneName = GetBoneTagName(id);
								char boneId[16];
								if(!boneName)
								{
									sprintf(boneId, "%u", (unsigned int)id);
									boneName = boneId;
								}

								sprintf(szBuffer, "%i,%s,%i,%s,%i,%s,%s,%i,%f\n",
									i, frameFilterDictionaryName,
									j, frameFilterName,
									k, trackName, boneName, index, weight);
								csvFile->WriteByte(szBuffer, istrlen(szBuffer));
							}
						}
					}
				}
			}
		}
	}

	// Close CSV file
	csvFile->Close();

	ASSET.PopFolder();
}

void CAnimViewer::LoadFilters()
{
	fwFrameFilterDictionaryStore::Load();
}

void CAnimViewer::SaveFilters()
{
	fwFrameFilterDictionaryStore::Save();
}

#endif // __DEV

void CAnimViewer::AddExpressionDebugWidgets()
{
	RemoveExpressionDebugWidgets();

	m_pExpressionWidgetGroup = m_pExpressionParentGroup->AddGroup("Expression widgets", false, NULL);

	if(m_pDynamicEntity)
	{
		bkGroup* pMidPhysicsGroup = m_pExpressionWidgetGroup->AddGroup("Mid Physics Expressions", false, NULL);
		m_debugMidPhysicsExpressions.AddExpressionDebugWidgets(*m_pDynamicEntity, *pMidPhysicsGroup);

		bkGroup* pPreRenderGroup = m_pExpressionWidgetGroup->AddGroup("Pre Render Expressions", false, NULL);
		m_debugPreRenderExpressions.AddExpressionDebugWidgets(*m_pDynamicEntity, *pPreRenderGroup);
	}
}

void CAnimViewer::RemoveExpressionDebugWidgets()
{
	m_debugMidPhysicsExpressions.RemoveExpressionDebugWidgets();
	m_debugPreRenderExpressions.RemoveExpressionDebugWidgets();

	if(m_pExpressionWidgetGroup)
	{
		m_pBank->DeleteGroup(*m_pExpressionWidgetGroup);
		m_pExpressionWidgetGroup = NULL;
	}
}

void CAnimViewer::UpdateExpressionDebugWidgets()
{
	if(m_pDynamicEntity && m_pDynamicEntity == m_debugMidPhysicsExpressions.m_pDynamicEntity)
	{
		m_debugMidPhysicsExpressions.UpdateExpressionDebugWidgets();
	}
	else if(m_debugMidPhysicsExpressions.m_pDynamicEntity)
	{
		m_debugMidPhysicsExpressions.RemoveExpressionDebugWidgets();
	}
	if(m_pDynamicEntity && m_pDynamicEntity == m_debugPreRenderExpressions.m_pDynamicEntity)
	{
		m_debugPreRenderExpressions.UpdateExpressionDebugWidgets();
	}
	else if(m_debugPreRenderExpressions.m_pDynamicEntity)
	{
		m_debugPreRenderExpressions.RemoveExpressionDebugWidgets();
	}
}

void CAnimViewer::ResetExpressionDebugWidgets()
{
	m_debugMidPhysicsExpressions.ResetExpressionDebugWidgets();
	m_debugPreRenderExpressions.ResetExpressionDebugWidgets();
}


#endif // __BANK
