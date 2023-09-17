// Copyright Sam Bonifacio 2021. All Rights Reserved.

#include "Simulation/TetherSimulationSegment.h"
#include "CableSplineUtils.h"
#include "TetherLogs.h"

TArray<FVector> FTetherSimulationSegment::GetParticleLocations() const
{
	TArray<FVector> Result;
	for(const FTetherSimulationParticle& Particle : Particles)
	{
		ensure(!Particle.Position.ContainsNaN());
		Result.Add(Particle.Position);
	}
	return Result;
}

bool FTetherSimulationSegment::IsValid() const
{
	return Length > 0.f;
}

void FTetherSimulationSegment::SetInvalidated(bool bInvalidate)
{
	if(!ensure(!IsRunningGame()))
	{
		// Should never be changing invalidation in a running game
		return;
	}
	
	if(bInvalidated != bInvalidate)
	{
		bInvalidated = bInvalidate;
		if(bInvalidated)
		{
			UE_LOG(LogTetherCable, Verbose, TEXT("Invalidated Segment %i"), SegmentUniqueId);
		}
		else
		{
			UE_LOG(LogTetherCable, Verbose, TEXT("Uninvalidated Segment %i"), SegmentUniqueId);
		}
	}
}

void FTetherSimulationSegment::BuildParticles(float ParticleSegmentLength, bool bStartFixed, bool bEndFixed)
{
	ensure(!SplineSegmentInfo.ContainsNaN());
	
	const FVector StartPos = SplineSegmentInfo.StartLocation;
	const FVector EndPos = SplineSegmentInfo.EndLocation;

	Particles.Reset();

	if(StartPos.Equals(EndPos))
	{
		return;
	}

	ensure(!StartPos.ContainsNaN());
	ensure(!EndPos.ContainsNaN());

	const int32 ReparamStepsPerSegment = 10;
	const FInterpCurveVector SplineCurve = FCableSplineUtils::SplineFromSegmentInfo(SplineSegmentInfo);
	const FInterpCurveFloat ReparamTable = FCableSplineUtils::ReparamTableFromSplineCurve(SplineCurve, ReparamStepsPerSegment);

	// Spline distance between the points of the segment
	const float SegmentSplineLength = ReparamTable.Points[ReparamStepsPerSegment].InVal;

	ensure(SegmentSplineLength > 0.f);

	// Length of the cable segment that should be actually simulated
	// Should be longer than the spline segment length
	const float ActualSegmentLength = FMath::Max(Length, SegmentSplineLength);

	// Ideal distance along the actual simulated length to create each particle
	const float DesiredParticleInterval = ParticleSegmentLength;

	// Ideal distance along the spline segment length to create each particle
	// Should be shorter than the desired particle interval
	const float DesiredParticleSplineInterval = DesiredParticleInterval * SegmentSplineLength / ActualSegmentLength;

	int32 NumParticlesForSegment = (SegmentSplineLength / DesiredParticleSplineInterval) + 1;

	// Must have at least 2 particles per segment, anything less doesn't make sense
	if(NumParticlesForSegment < 2)
	{
		NumParticlesForSegment = 2;
	}

	const float ActualParticleSplineInterval = SegmentSplineLength / (NumParticlesForSegment - 1);

	Particles.AddDefaulted(NumParticlesForSegment);

	for(int i=0;i<NumParticlesForSegment;i++)
	{
		const float ParticleDistance = ActualParticleSplineInterval * i;
		const float DistancePercent = ParticleDistance / SegmentSplineLength;
		ensure(!FMath::IsNaN(DistancePercent));
		const FVector ParticlePos = SplineCurve.Eval(DistancePercent, FVector::ZeroVector);
		//::DrawDebugPoint(GetWorld(), ParticlePos, 20.f, FColor::Green, false, 10.f);

		FTetherSimulationParticle& Particle = Particles[i];
		
		Particle.bFree = true;
		if((i==0 && bStartFixed) ||
			(i==NumParticlesForSegment-1 && bEndFixed))
		{
			Particle.bFree = false;
		}
		
		Particle.Position = ParticlePos;
		Particle.OldPosition = ParticlePos;
		ensure(!Particle.Position.ContainsNaN());

		if(i > 0)
		{
			// Ensure that position is different from previous particle
			const FTetherSimulationParticle& PreviousParticle = Particles[i - 1];
			ensureMsgf(Particle.Position != PreviousParticle.Position,
				TEXT("Particle %i position %s matches previous particle %s\n\
				SplineSegmentInfo: %s\n\
				SplineCurve num points: %i\n\
				SegmentSplineLength: %f\n\
				NumParticlesForSegment: %i\n\
				DistancePercent: %f\n\
				"), i, *Particle.Position.ToString(), *PreviousParticle.Position.ToString(),
				*SplineSegmentInfo.ToString(),
				SplineCurve.Points.Num(),
				SegmentSplineLength,
				NumParticlesForSegment,
				DistancePercent);
		}
	}

	SimulationTime = 0.f;
}
