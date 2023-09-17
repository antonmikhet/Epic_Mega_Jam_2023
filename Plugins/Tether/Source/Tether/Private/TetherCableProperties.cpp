// Copyright Sam Bonifacio 2021. All Rights Reserved.

#include "TetherCableProperties.h"

#include "Mesh/TMG_Basic.h"

FTetherCableProperties::FTetherCableProperties()
{
    //MeshGenerator = NewObject<UTMG_Basic>();
}

float FTetherCableProperties::GetDesiredParticleDistance() const
{
    return GetCollisionWidth() * SimulationOptions.ParticleDistanceScale;
}

float FTetherCableProperties::GetCollisionWidth() const
{
    return CableWidth * SimulationOptions.CollisionWidthScale;
}
