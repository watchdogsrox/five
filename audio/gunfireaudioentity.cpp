#include "audio/northaudioengine.h"
#include "audioengine/engine.h"
#include "audiohardware/driverdefs.h"
#include "audiohardware/driverutil.h"
#include "audiohardware/driverutil.h"
#include "camera/CamInterface.h"
#include "fwsys/timer.h"
#include "gunfireaudioentity.h"
#include "Peds/ped.h"
#include "scene/world/GameWorld.h"
#if __BANK
#include "bank/bkmgr.h"
#include "bank/bank.h"
#include "bank/combo.h"
#include "bank/slider.h"
#include "grcore/debugdraw.h"
#endif

AUDIO_WEAPONS_OPTIMISATIONS() 

//-------------------------------------------------------------------------------------------------------------------
bank_float g_AmountOfEnergyDecay = 0.2f;
bank_float g_DeltaTimeToSpikeNearbyZones = 250.f;
bank_float g_MinTimeToSpikeNearbyZones = 1000.f;
bank_float g_MaxTimeToSpikeNearbyZones = 3000.f;
bank_float g_PropagationDamping = 0.250f; 
bank_u32 g_TimeToSpikeNearbyZones = 1000;
bank_u32 g_Origin = 3;
const u32 g_DefaultDefinition = 3;
#if __BANK
u32 audGunFireAudioEntity::g_Definition = g_DefaultDefinition;
bool audGunFireAudioEntity::g_DrawOverallGrid = true ;
bool audGunFireAudioEntity::g_DrawGunFightGrid = false;
bool g_DisplayEnergy = false;
bool g_DisplayCoordinates = false;
bool g_DisplayEnergyGraphs = false;
#endif
audSoundSet	audGunFireAudioEntity::sm_TailSounds;
//-------------------------------------------------------------------------------------------------------------------
void audGunFireAudioEntity::Init(){
	// Init the audEntity
	naAudioEntity::Init();
	// Init sound references 
	sm_TailSounds.Init("GUN_TAILS");
	CreateSound_PersistentReference(sm_TailSounds.Find("FRONT_LEFT"), &m_TailSounds[AUD_SECTOR_FL]);
	CreateSound_PersistentReference(sm_TailSounds.Find("FRONT_RIGHT"), &m_TailSounds[AUD_SECTOR_FR]);
	CreateSound_PersistentReference(sm_TailSounds.Find("REAR_LEFT"), &m_TailSounds[AUD_SECTOR_RL]);
	CreateSound_PersistentReference(sm_TailSounds.Find("REAR_RIGHT"), &m_TailSounds[AUD_SECTOR_RR]);
	// Init all the necessary params (energy, volume, damping factors ...)
	m_NumberOfSquaresInOverallGridSquare = AUD_NUM_SECTORS_ACROSS/BANK_SWITCH(g_Definition,g_DefaultDefinition);
	for (u32 i = 0 ; i < 4 ; ++i )
	{
		m_TailVolume[i] = 0.f;
		m_OverallTailEnergy[i] = 0.f;
		m_EnergyFlowsSmoother[i].Init(0.01f,true);
		if(naVerifyf(m_TailSounds[i],"Missing gun tail sound.")){  
			m_TailSounds[i]->SetRequestedVolume(-100.f);
			m_TailSounds[i]->SetReleaseTime(200);
			m_TailSounds[i]->PrepareAndPlay();
		}
	}	

	for (u32 x = 0 ; x < AUD_NUM_SECTORS_ACROSS ; ++x )
	{
		m_TimeToSpikeNearbyZones[x] = float(fwTimer::GetTimeInMilliseconds());
		for (u32 j = 0 ; j < AUD_MAX_NUM_ENERGY_DELAY_EVENTS ; ++j )
		{
			m_EnergyDelayEvents[x][j].hasToTriggerEvent = false;
			m_EnergyDelayEvents[x][j].evenTime = 0.f;
			m_EnergyDelayEvents[x][j].tailEnergy = 0.f;
		}
		for ( u32 y = 0; y <AUD_NUM_SECTORS_ACROSS; ++y )
		{
			u32 sector = y + AUD_NUM_SECTORS_ACROSS*x;
			m_TailEnergy[sector] = 0.f;
			m_OldTailEnergy[sector] = 0.f;
		}
	}
	// Init curves.
	m_AmountOfTailEnergyAffects.Init("AMOUNT_OF_TAIL_ENERGY_AFFECTS");
	m_DistanceDumpFactor.Init("DISTANCE_DUMP_FACTOR");
	m_EnergyDecay.Init("ENERGY_DECAY");
	m_EnergyToEnergyDecay.Init("ENERGY_TO_ENERGY_DECAY");
	m_BuiltUpFactorToEnergy.Init("BUILT_UP_FACTOR_TO_ENERGY");
	m_BuiltUpFactorToSourceEnergy.Init("BUILT_UP_FACTOR_TO_SOURCE_ENERGY");
	m_EnergyToVolume.Init("ENERGY_TO_VOLUME");
}
//-------------------------------------------------------------------------------------------------------------------
#if __BANK
void audGunFireAudioEntity::InitGunFireGrid(){
	// Initialize the sectors coordinates.
	const Matrix34 listenerMatrix = MAT34V_TO_MATRIX34(g_AudioEngine.GetEnvironment().GetPanningListenerMatrix());
	const u32 sectorIndex = static_cast<u32>(audNorthAudioEngine::GetGtaEnvironment()->ComputeWorldSectorIndex(listenerMatrix.d));
	const u32 sectorX = sectorIndex % g_audNumWorldSectorsX, sectorY = (sectorIndex-sectorX)/g_audNumWorldSectorsY;
	// Work out where the centre of the main sector is, so we know how much of the others to factor in
	f32 mainSectorX = g_audWidthOfSector * (sectorX - (g_fAudNumWorldSectorsX/2.f) + 0.5f);
	f32 mainSectorY = g_audDepthOfSector * (sectorY - (g_fAudNumWorldSectorsY/2.f) + 0.5f);
	Vector3 origin,gridControl;
	origin.SetX(mainSectorX - g_audWidthOfSector/g_Origin * (0.5f * AUD_NUM_SECTORS_ACROSS));
	origin.SetY(mainSectorY + g_audDepthOfSector/g_Origin * (0.5f * AUD_NUM_SECTORS_ACROSS));
	origin.SetZ(0.f);
	gridControl = origin;
	m_GunFireSectorsCoords[0].xCoord = origin;
	m_GunFireSectorsCoords[0].yCoord = origin;
	for(u32 i = 1 ; i <= AUD_NUM_SECTORS_ACROSS ; ++i)
	{
		gridControl.SetY(gridControl.GetY() - g_audDepthOfSector/g_Definition);
		m_GunFireSectorsCoords[i].xCoord = gridControl;
	}
	gridControl = origin;
	for(u32 i = 1 ; i <= AUD_NUM_SECTORS_ACROSS ; ++i)
	{
		gridControl.SetX(gridControl.GetX() + g_audWidthOfSector/g_Definition);
		m_GunFireSectorsCoords[i].yCoord = gridControl;
	}
	m_NumberOfSquaresInOverallGridSquare = AUD_NUM_SECTORS_ACROSS/g_Definition;
	Vector3 oOrigin,oGridControl;
	oOrigin.SetX(mainSectorX - g_audWidthOfSector * (0.5f * AUD_NUM_SECTORS_ACROSS));
	oOrigin.SetY(mainSectorY + g_audDepthOfSector * (0.5f * AUD_NUM_SECTORS_ACROSS));
	oOrigin.SetX(oOrigin.GetX() + g_audWidthOfSector * 0.5f );
	oOrigin.SetY(oOrigin.GetY() - g_audDepthOfSector * 0.5f );
	oOrigin.SetZ(0.f);
	oGridControl = oOrigin;
	for(u32 y = 0 ; y < AUD_NUM_SECTORS_ACROSS ; ++y)
	{
		for(u32 x = 0 ; x < AUD_NUM_SECTORS_ACROSS ; ++x)
		{
			m_OverallGridCenters[x+AUD_NUM_SECTORS_ACROSS*y] = oGridControl;
			oGridControl.SetX(oGridControl.GetX() + g_audWidthOfSector);
		}
		oGridControl.SetX(oOrigin.GetX());
		oGridControl.SetY(oGridControl.GetY() - g_audDepthOfSector);
	}
}
#endif
//-------------------------------------------------------------------------------------------------------------------
void audGunFireAudioEntity::ShutDown()
{
	audEntity::Shutdown();
}
//-------------------------------------------------------------------------------------------------------------------
void audGunFireAudioEntity::PreUpdateService(u32 timeInMs)
{
	TriggerGunTailsDelayEvents(timeInMs);
}
//-------------------------------------------------------------------------------------------------------------------
void audGunFireAudioEntity::Update(u32 timeInMs)
{
	// Compute the volume for the four loops based on the overall energy of the grid.
	ComputeGunTailsVolume();

	PostGameUpdate(timeInMs);
}
//-------------------------------------------------------------------------------------------------------------------
void audGunFireAudioEntity::PostGameUpdate(u32 timeInMs)
{
	AddGunTailsDelayEvents(timeInMs);
}
//-------------------------------------------------------------------------------------------------------------------
// Gun tails functions.
//-------------------------------------------------------------------------------------------------------------------
void audGunFireAudioEntity::AddGunTailsDelayEvents(u32 timeInMs)
{
	// At the end of the frame analyze what's happened and based on that trigger future events
	for (s32 i=0; i  <AUD_NUM_SECTORS; ++i)
	{ 
		// If the energy of the sector has been increased has to spike the nearby zones.
		if( m_TailEnergy[i] > 0.f && (m_TailEnergy[i] > m_OldTailEnergy[i]))
		{
			f32 dampedEnergy = m_TailEnergy[i] - m_OldTailEnergy[i];
			//Make sure we don't go out of the limit.
			if( dampedEnergy <= 0.f )
				dampedEnergy = 0.f;
			// The energy that the zone transfers to  it neighbours is a factor of how much this energy increase since last frame, affected by the built up factor. 
			f32 spikeEnergy = /*m_EnergyDecay.CalculateValue*/(dampedEnergy * g_PropagationDamping) * m_BuiltUpFactorToEnergy.CalculateValue(audNorthAudioEngine::GetGtaEnvironment()->GetBuiltUpFactorOfSector(i));
			SpikeNearByZones( i, spikeEnergy, timeInMs);
		}
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audGunFireAudioEntity::TriggerGunTailsDelayEvents(u32 timeInMs)
{
	// Check if I have to trigger any delay event.
	for (u32 i = 0 ; i < AUD_NUM_SECTORS ; ++i )
	{
		// At the beginning of the frame store the last energy.
		m_OldTailEnergy[i] = m_TailEnergy[i];
		for (u32 j = 0 ; j < AUD_MAX_NUM_ENERGY_DELAY_EVENTS ; ++j )
		{
			//If there is an event to trigger, update the corresponding energy and empty the slot for future events.
			if(m_EnergyDelayEvents[i][j].hasToTriggerEvent)
			{
				if(timeInMs >= m_EnergyDelayEvents[i][j].evenTime)
				{
					m_TailEnergy[i] += m_AmountOfTailEnergyAffects.CalculateValue(m_TailEnergy[i]) * m_EnergyDelayEvents[i][j].tailEnergy;
					m_EnergyDelayEvents[i][j].hasToTriggerEvent = false;
					m_EnergyDelayEvents[i][j].tailEnergy = 0.f;
					m_EnergyDelayEvents[i][j].evenTime = 0.f;
				}
			}
		}
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audGunFireAudioEntity::ComputeGunTailsVolume()
{
	// Naturally decrease the energy over time.
	for (u32 i = 0 ; i < AUD_NUM_SECTORS ; ++i )
	{
		m_TailEnergy[i] -= m_EnergyToEnergyDecay.CalculateValue(m_TailEnergy[i]) * g_AmountOfEnergyDecay * fwTimer::GetTimeStep();
		m_OldTailEnergy[i] = (m_TailEnergy[i] <= 0.f) ? 0.f : m_OldTailEnergy[i]; 
		m_TailEnergy[i] = Clamp(m_TailEnergy[i], 0.f, 1.f );

	}
	// Reset the overall energy.
	for (u32 i = 0 ; i < 4 ; ++i )
	{
		m_OverallTailEnergy[i] = 0.f;
	}
	const Matrix34 listenerMatrix = MAT34V_TO_MATRIX34(g_AudioEngine.GetEnvironment().GetPanningListenerMatrix());
	Vector3 leftSpeaker = 0.5f * (listenerMatrix.b - listenerMatrix.a);
	leftSpeaker.SetZ(0.f);
	leftSpeaker.NormalizeSafe();
	Vector3 rightSpeaker = 0.5f * (listenerMatrix.b + listenerMatrix.a);
	rightSpeaker.SetZ(0.f);
	rightSpeaker.NormalizeSafe();
	Vector3 listenerPos2D =listenerMatrix.d;
	listenerPos2D.SetZ(0.f);
	// Initialize the sectors coordinates.
	const u32 sectorIndex = static_cast<u32>(audNorthAudioEngine::GetGtaEnvironment()->ComputeWorldSectorIndex(listenerMatrix.d));
	const u32 sectorX = sectorIndex % g_audNumWorldSectorsX, sectorY = (sectorIndex-sectorX)/g_audNumWorldSectorsY;
	// Work out where the centre of the main sector is, so we know how much of the others to factor in
	f32 mainSectorX = g_audWidthOfSector * (sectorX - (g_fAudNumWorldSectorsX/2.f) + 0.5f);
	f32 mainSectorY = g_audDepthOfSector * (sectorY - (g_fAudNumWorldSectorsY/2.f) + 0.5f);
	Vector3 oOrigin,oGridControl;
	oOrigin.SetX(mainSectorX - g_audWidthOfSector * (0.5f * AUD_NUM_SECTORS_ACROSS));
	oOrigin.SetY(mainSectorY + g_audDepthOfSector * (0.5f * AUD_NUM_SECTORS_ACROSS));
	oOrigin.SetX(oOrigin.GetX() + g_audWidthOfSector * 0.5f );
	oOrigin.SetY(oOrigin.GetY() - g_audDepthOfSector * 0.5f );
	oOrigin.SetZ(0.f);
	oGridControl = oOrigin;
	// Each sector of the grid must contribute to the overall energy.  Divide the grid in 4 overall energies.
	for (u32 x = 0 ; x < AUD_NUM_SECTORS_ACROSS ; ++x )
	{
		for ( u32 y = 0; y <AUD_NUM_SECTORS_ACROSS; ++y )
		{
			u32 sector = y + AUD_NUM_SECTORS_ACROSS*x;
			f32 diffx,diffy;
			diffx = (f32)AUD_HALF_NUM_SECTORS_ACROSS - (f32)x;
			diffy = (f32)AUD_HALF_NUM_SECTORS_ACROSS - (f32)y;
			if( diffx == 0.f )   diffx = 1.f ;
			if( diffy == 0.f )   diffy = 1.f ;
			float dampingFactor = 1/fabs(diffx) * 1/fabs(diffy) ;
			// Calculate based on the listener front how the energy has to travel between speakers.
			Vector3 dirToZoneCenter = oGridControl - listenerPos2D;
			dirToZoneCenter.NormalizeSafe();
			f32 xDotAngle = leftSpeaker.Dot(dirToZoneCenter);
			f32 yDotAngle = rightSpeaker.Dot(dirToZoneCenter);
//#if __BANK 
//			grcDebugDraw::Line(listenerPos2D + Vector3(0.f,0.f,100.f),oGridControl + Vector3(0.f,0.f,100.f),Color_black);
//			char text[64];
//			formatf(text,sizeof(text),"%f \n %f",xDotAngle, yDotAngle); 
//			grcDebugDraw::Text(m_OverallGridCenters[sector] + Vector3(0.0f,0.0f,1.6f),Color_pink,text,true,1);
//#endif
			oGridControl.SetX(oGridControl.GetX() + g_audWidthOfSector);
			// Has to affect FL & FR speaker
			if( xDotAngle >= 0.f && yDotAngle >= 0.f)
			{
				m_OverallTailEnergy[AUD_SECTOR_FL] += dampingFactor * m_TailEnergy[sector] *	xDotAngle;
				m_OverallTailEnergy[AUD_SECTOR_FR] += dampingFactor * m_TailEnergy[sector] *	yDotAngle;
			}
			// Has to affect FL & RL speaker
			else if (xDotAngle >= 0.f)
			{
				m_OverallTailEnergy[AUD_SECTOR_FL] += dampingFactor * m_TailEnergy[sector] *	xDotAngle;
				m_OverallTailEnergy[AUD_SECTOR_RL] += dampingFactor * m_TailEnergy[sector] *	(-yDotAngle);

			}
			// Has to affect FR & RR speaker
			else if (yDotAngle >= 0.f)
			{
				m_OverallTailEnergy[AUD_SECTOR_FR] += dampingFactor * m_TailEnergy[sector] *	yDotAngle;
				m_OverallTailEnergy[AUD_SECTOR_RR] += dampingFactor * m_TailEnergy[sector] *	(-xDotAngle);
			}
			// Has to affect RL & RR speaker
			else
			{
				m_OverallTailEnergy[AUD_SECTOR_RL] += dampingFactor * m_TailEnergy[sector] *	(-yDotAngle);
				m_OverallTailEnergy[AUD_SECTOR_RR] += dampingFactor * m_TailEnergy[sector] *	(-xDotAngle);

			}
		}
		oGridControl.SetX(oOrigin.GetX());
		oGridControl.SetY(oGridControl.GetY() - g_audDepthOfSector);
	}        

	for (u32 i = 0; i < 4; ++i) 
	{
		m_OverallTailEnergy[i] = m_EnergyFlowsSmoother[i].CalculateValue(m_OverallTailEnergy[i]);
		if(naVerifyf(m_TailSounds[i],"Missing gun tail sound.")){  
			m_TailSounds[i]->SetRequestedVolume(audDriverUtil::ComputeDbVolumeFromLinear(m_EnergyToVolume.CalculateValue(m_OverallTailEnergy[i])));
		}
	}


#if __BANK
	if (g_DisplayEnergy)
	{
		for(u32 i = 0 ; i < AUD_NUM_SECTORS ; ++i)
		{
			if(m_TailEnergy[i] > 0.f)
			{
				char text[64];
				formatf(text,sizeof(text),"%f \n %f", m_TailEnergy[i], m_OldTailEnergy[i]); 
				grcDebugDraw::Text(m_OverallGridCenters[i] + Vector3(0.0f,0.0f,1.6f), Color32(255, 255, 255, 255),text,true,1);
			}
		}
	}


	if (g_DisplayEnergyGraphs)
	{
		static const char *topLeft = "Front Left";
		static f32 topLeftEnergy;
		topLeftEnergy = m_OverallTailEnergy[AUD_SECTOR_FL];
		static const char *topRight = "Front Right";
		static f32 topRightEnergy;
		topRightEnergy = m_OverallTailEnergy[AUD_SECTOR_FR];
		static const char *backLeft = "Rear Left";
		static f32 backLeftEnergy;
		backLeftEnergy = m_OverallTailEnergy[AUD_SECTOR_RL];
		static const char *backRight = "Rear Right";
		static f32 backRightEnergy;
		backRightEnergy = m_OverallTailEnergy[AUD_SECTOR_RR];
		static audMeterList meterList[4];
		meterList[0].left = 200;
		meterList[0].bottom = 200;
		meterList[0].width = 10.f;
		meterList[0].height = 50.f;
		meterList[0].values = &topLeftEnergy;
		meterList[0].names = &topLeft;
		meterList[0].numValues = 1;
		audNorthAudioEngine::DrawLevelMeters(&(meterList[0])); 
		meterList[1].left = 300;
		meterList[1].bottom = 200;
		meterList[1].width = 10.f;
		meterList[1].height = 50.f;
		meterList[1].values = &topRightEnergy;
		meterList[1].names = &topRight;
		meterList[1].numValues = 1;
		audNorthAudioEngine::DrawLevelMeters(&(meterList[1])); 
		meterList[2].left = 200;
		meterList[2].bottom = 400;
		meterList[2].width = 10.f;
		meterList[2].height = 50.f;
		meterList[2].values = &backLeftEnergy;
		meterList[2].names = &backLeft;
		meterList[2].numValues = 1;
		audNorthAudioEngine::DrawLevelMeters(&(meterList[2])); 
		meterList[3].left = 300;
		meterList[3].bottom = 400;
		meterList[3].width = 10.f;
		meterList[3].height = 50.f;
		meterList[3].values = &backRightEnergy;
		meterList[3].names = &backRight;
		meterList[3].numValues = 1;
		audNorthAudioEngine::DrawLevelMeters(&(meterList[3])); 
	}
#endif
}
//-------------------------------------------------------------------------------------------------------------------
void audGunFireAudioEntity::AddGunFireEvent(const Vector3 &shooterPos, f32 weaponTailEnergy){

	//Work out in which sector the shooter is.
	Vector3 shooterPos2D = shooterPos ; 
	shooterPos2D.SetZ(0.f);
	Vector3 playerPos = VEC3V_TO_VECTOR3(CGameWorld::FindLocalPlayer()->GetTransform().GetPosition());
	playerPos.SetZ(0.f);
	f32 distanceToPlayer = shooterPos2D.Dist(playerPos);
	distanceToPlayer = Clamp<float>(distanceToPlayer, 0.f, DSECTOR_HYPOTENUSE);
	distanceToPlayer /= DSECTOR_HYPOTENUSE;
	const Matrix34 listenerMatrix = MAT34V_TO_MATRIX34(g_AudioEngine.GetEnvironment().GetPanningListenerMatrix());
	const u32 sectorIndex = static_cast<u32>(audNorthAudioEngine::GetGtaEnvironment()->ComputeWorldSectorIndex(listenerMatrix.d));
	const u32 sectorX = sectorIndex % g_audNumWorldSectorsX, sectorY = (sectorIndex-sectorX)/g_audNumWorldSectorsY;
	f32 mainSectorX = g_audWidthOfSector * (sectorX - (g_fAudNumWorldSectorsX/2.f) + 0.5f);
	f32 mainSectorY = g_audDepthOfSector * (sectorY - (g_fAudNumWorldSectorsY/2.f) + 0.5f);
	Vector3 origin;
	origin.SetX(mainSectorX - g_audWidthOfSector/g_Origin * (0.5f * AUD_NUM_SECTORS_ACROSS));
	origin.SetY(mainSectorY + g_audDepthOfSector/g_Origin * (0.5f * AUD_NUM_SECTORS_ACROSS));
	origin.SetZ(0.f);
	if((shooterPos2D.GetY() > origin.GetY()) || (shooterPos2D.GetY() < (origin.GetY() - AUD_NUM_SECTORS_ACROSS * g_audDepthOfSector/g_Origin)))
	{
		return;
	}
	if((shooterPos2D.GetX() < origin.GetX()) || (shooterPos2D.GetX() > (origin.GetX() + AUD_NUM_SECTORS_ACROSS * g_audWidthOfSector/g_Origin)))
	{
		return;
	}
	f32 xDistanceToGridOrigin = fabs(shooterPos2D.GetX() - origin.GetX());
	f32 yDistanceToGridOrigin = fabs(shooterPos2D.GetY() - origin.GetY());
	s32 xCoord = -1,yCoord = -1,overallXCoord = -1,overallYCoord = -1;
	yCoord = (s32)xDistanceToGridOrigin/((s32)g_audWidthOfSector/g_Origin);
	xCoord = (s32)yDistanceToGridOrigin/((s32)g_audDepthOfSector/g_Origin);
	
	if (m_NumberOfSquaresInOverallGridSquare)
	{
		overallXCoord = xCoord / m_NumberOfSquaresInOverallGridSquare;
		overallYCoord = yCoord / m_NumberOfSquaresInOverallGridSquare;
		u32 gunFightSector = overallYCoord + m_NumberOfSquaresInOverallGridSquare * overallXCoord;
		s32 overallSector =  AUD_CENTER_SECTOR - (AUD_NUM_SECTORS_ACROSS + 1) + overallYCoord + overallXCoord * AUD_NUM_SECTORS_ACROSS;
		SpikeEnergy(overallSector,gunFightSector, weaponTailEnergy, distanceToPlayer);
	}
#if __BANK
	if(g_DisplayCoordinates){
		char text[256];
		formatf(text,sizeof(text),"GunFight -> [%d,%d] Overall -> [%d,%d]", xCoord,yCoord, overallXCoord,overallYCoord); 
		grcDebugDraw::Text(shooterPos + Vector3(0.0f,0.0f,1.6f), Color32(255, 255, 255, 255),text,true,100);
	}
#endif
}
//-------------------------------------------------------------------------------------------------------------------
void audGunFireAudioEntity::SpikeEnergy(s32 sector, u32 gunFightsector,f32 weaponTailEnergy, f32 distanceToPlayer)
{
	// Spike the energy of the sector based on the weapon tail energy and the distance. 
	m_TailEnergy[sector]  += m_AmountOfTailEnergyAffects.CalculateValue(m_TailEnergy[sector]) * weaponTailEnergy * m_DistanceDumpFactor.CalculateValue(distanceToPlayer);
	m_TailEnergy[sector] = Clamp<float>(m_TailEnergy[sector], 0.f, 1.f);
	// By setting the old energy here for the gunfight sectors then in the postUpdateService we don't trigger future events again.
	m_OldTailEnergy[sector] = m_TailEnergy[sector];
	// If is time to spike the nearby zones do it.
	if( fwTimer::GetTimeInMilliseconds() >= m_TimeToSpikeNearbyZones[gunFightsector] )
	{
		if(	m_TailEnergy[sector] > 0.f )
		{
			f32 spikeEnergy = m_TailEnergy[sector] * g_PropagationDamping * m_BuiltUpFactorToEnergy.CalculateValue(audNorthAudioEngine::GetGtaEnvironment()->GetBuiltUpFactorOfSector(sector));
			SpikeNearByZones( sector, spikeEnergy, fwTimer::GetTimeInMilliseconds() + g_TimeToSpikeNearbyZones);
		}
		// Get a new random time to spike.
		m_TimeToSpikeNearbyZones[gunFightsector] = fwTimer::GetTimeInMilliseconds() + audEngineUtil::GetRandomNumberInRange(g_MinTimeToSpikeNearbyZones, g_MaxTimeToSpikeNearbyZones);
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audGunFireAudioEntity::SpikeNearByZones(s32 sector,f32 energy, u32 timeInMs)
{
	u32 x,y; 
	x = (s32)sector / (s32)AUD_NUM_SECTORS_ACROSS;
	y = (s32)(sector - ((s32)x * AUD_NUM_SECTORS_ACROSS));
	s32 overallSector = (s32)sector;
	// -----------------------------------------------------------------------------TOP LEFT 
	if( y != 0) // If we are not on the left column.
	{	
		overallSector = sector - (AUD_NUM_SECTORS_ACROSS + 1);
		if(overallSector >= 0)// If we are inside the limits.
		{
			for (u32 i = 0 ; i < AUD_MAX_NUM_ENERGY_DELAY_EVENTS ; ++i )
			{
				if(m_EnergyDelayEvents[overallSector][i].hasToTriggerEvent == false)
				{
					m_EnergyDelayEvents[overallSector][i].hasToTriggerEvent = true;
					m_EnergyDelayEvents[overallSector][i].evenTime = timeInMs  + (g_TimeToSpikeNearbyZones + i * g_DeltaTimeToSpikeNearbyZones);
					m_EnergyDelayEvents[overallSector][i].tailEnergy = energy;
					break;			
				}
			}
		}
		// -----------------------------------------------------------------------------LEFT 
		overallSector = sector -  1;
		if(overallSector >= 0)
		{
			for (u32 i = 0 ; i < AUD_MAX_NUM_ENERGY_DELAY_EVENTS ; ++i )
			{
				if(m_EnergyDelayEvents[overallSector][i].hasToTriggerEvent == false)
				{
					m_EnergyDelayEvents[overallSector][i].hasToTriggerEvent = true;
					m_EnergyDelayEvents[overallSector][i].evenTime = timeInMs  + (g_TimeToSpikeNearbyZones + i * g_DeltaTimeToSpikeNearbyZones);
					m_EnergyDelayEvents[overallSector][i].tailEnergy = energy;
					break;			
				}
			}
		}
		// -----------------------------------------------------------------------------BOTTOM LEFT  
		overallSector = sector + (AUD_NUM_SECTORS_ACROSS - 1);
		if( overallSector < AUD_NUM_SECTORS)
		{
			for (u32 i = 0 ; i < AUD_MAX_NUM_ENERGY_DELAY_EVENTS ; ++i )
			{
				if(m_EnergyDelayEvents[overallSector][i].hasToTriggerEvent == false)
				{
					m_EnergyDelayEvents[overallSector][i].hasToTriggerEvent = true;
					m_EnergyDelayEvents[overallSector][i].evenTime = timeInMs  + (g_TimeToSpikeNearbyZones + i * g_DeltaTimeToSpikeNearbyZones);
					m_EnergyDelayEvents[overallSector][i].tailEnergy = energy;
					break;			
				}
			}
		}
	}

	// -----------------------------------------------------------------------------TOP RIGHT  
	if( y != (AUD_NUM_SECTORS_ACROSS - 1)) // If we are not on the right column.
	{	
		overallSector = sector - (AUD_NUM_SECTORS_ACROSS - 1);
		if(overallSector >= 0)// If we are inside the limits.
		{
			for (u32 i = 0 ; i < AUD_MAX_NUM_ENERGY_DELAY_EVENTS ; ++i )
			{
				if(m_EnergyDelayEvents[overallSector][i].hasToTriggerEvent == false)
				{
					m_EnergyDelayEvents[overallSector][i].hasToTriggerEvent = true;
					m_EnergyDelayEvents[overallSector][i].evenTime = timeInMs  + (g_TimeToSpikeNearbyZones + i * g_DeltaTimeToSpikeNearbyZones);
					m_EnergyDelayEvents[overallSector][i].tailEnergy = energy;
					break;			
				}
			}
		}
		// -----------------------------------------------------------------------------RIGHT 
		overallSector = sector  + 1;
		if(overallSector < AUD_NUM_SECTORS)
		{
			for (u32 i = 0 ; i < AUD_MAX_NUM_ENERGY_DELAY_EVENTS ; ++i )
			{
				if(m_EnergyDelayEvents[overallSector][i].hasToTriggerEvent == false)
				{
					m_EnergyDelayEvents[overallSector][i].hasToTriggerEvent = true;
					m_EnergyDelayEvents[overallSector][i].evenTime = timeInMs  + (g_TimeToSpikeNearbyZones + i * g_DeltaTimeToSpikeNearbyZones);
					m_EnergyDelayEvents[overallSector][i].tailEnergy = energy;
					break;			
				}
			}
		}
		// -----------------------------------------------------------------------------BOTTOM RIGHT  
		overallSector = sector + (AUD_NUM_SECTORS_ACROSS + 1);
		if(overallSector < AUD_NUM_SECTORS)
		{
			for (u32 i = 0 ; i < AUD_MAX_NUM_ENERGY_DELAY_EVENTS ; ++i )
			{
				if(m_EnergyDelayEvents[overallSector][i].hasToTriggerEvent == false)
				{
					m_EnergyDelayEvents[overallSector][i].hasToTriggerEvent = true;
					m_EnergyDelayEvents[overallSector][i].evenTime = timeInMs  + (g_TimeToSpikeNearbyZones + i * g_DeltaTimeToSpikeNearbyZones);
					m_EnergyDelayEvents[overallSector][i].tailEnergy = energy;
					break;			
				}
			}
		}
	}
	// -----------------------------------------------------------------------------BOTTOM MID  
	overallSector = sector + AUD_NUM_SECTORS_ACROSS;
	if(overallSector < AUD_NUM_SECTORS)
	{
		for (u32 i = 0 ; i < AUD_MAX_NUM_ENERGY_DELAY_EVENTS ; ++i )
		{
			if(m_EnergyDelayEvents[overallSector][i].hasToTriggerEvent == false)
			{
				m_EnergyDelayEvents[overallSector][i].hasToTriggerEvent = true;
				m_EnergyDelayEvents[overallSector][i].evenTime = timeInMs  + (g_TimeToSpikeNearbyZones + i * g_DeltaTimeToSpikeNearbyZones);
				m_EnergyDelayEvents[overallSector][i].tailEnergy = energy;
				break;			
			}
		}
	}
	// -----------------------------------------------------------------------------TOP MID  
	overallSector = sector - AUD_NUM_SECTORS_ACROSS;
	if(overallSector >= 0)// If we are inside the limits.
	{
		for (u32 i = 0 ; i < AUD_MAX_NUM_ENERGY_DELAY_EVENTS ; ++i )
		{
			if(m_EnergyDelayEvents[overallSector][i].hasToTriggerEvent == false)
			{
				m_EnergyDelayEvents[overallSector][i].hasToTriggerEvent = true;
				m_EnergyDelayEvents[overallSector][i].evenTime = timeInMs  + (g_TimeToSpikeNearbyZones + i * g_DeltaTimeToSpikeNearbyZones);
				m_EnergyDelayEvents[overallSector][i].tailEnergy = energy;
				break;			
			}
		}
	}
}
//-------------------------------------------------------------------------------------------------------------------
#if __BANK
void audGunFireAudioEntity::AddWidgets(bkBank &bank){

	bank.PushGroup("Gun Fights",false);
	bank.AddToggle("Display energy",   &g_DisplayEnergy);
	bank.AddToggle("Display coords",   &g_DisplayCoordinates);
	bank.AddToggle("Display energy graphs",   &g_DisplayEnergyGraphs);
	bank.AddToggle("Draw overall grid",   &g_DrawOverallGrid);
	bank.AddToggle("Draw gun fight grid", &g_DrawGunFightGrid);
	bank.AddSlider("Definition of the gun fight grid", &g_Definition, 1, AUD_NUM_SECTORS_ACROSS, 1);
	bank.AddSlider("Origin", &g_Origin, 1, AUD_NUM_SECTORS_ACROSS, 1);
	bank.PushGroup("Gun Tails",false);
		bank.AddSlider("g_AmountOfEnergyDecay", &g_AmountOfEnergyDecay, 0.0f, 1.0f, 0.001f);
		bank.AddSlider("g_TimeToSpikeNearbyZones", &g_TimeToSpikeNearbyZones, 0, 1000,50);
		bank.AddSlider("g_DeltaTimeToSpikeNearbyZones", &g_DeltaTimeToSpikeNearbyZones, 0.0f, 1000.0f,50.f);
		bank.AddSlider("g_MinTimeToSpikeNearbyZones", &g_MinTimeToSpikeNearbyZones, 0.f, 50000.f,50.f);
		bank.AddSlider("g_MaxTimeToSpikeNearbyZones", &g_MaxTimeToSpikeNearbyZones, 0.f, 50000.f,50.f);
		bank.AddSlider("g_PropagationDamping", &g_PropagationDamping, 0.f, 1.f,0.01f);
	bank.PopGroup();
	bank.PopGroup();
}
#endif