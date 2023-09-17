// Copyright Sam Bonifacio 2021. All Rights Reserved.

#include "Simulation/TetherSimulation.h"
#include "DrawDebugHelpers.h"
#include "PhysXInterfaceWrapper.h"
#include "Simulation/TetherSimulationModel.h"
#include "Simulation/TetherSimulationParams.h"
#include "Engine/World.h"
#include "Physics/PhysicsInterfaceCore.h"
#include "Simulation/TetherPhysicsUtils.h"
#include "Simulation/TetherSimulationContext.h"
#include "TaskTypes.h"
#include "Engine/TriggerBase.h"
#include "Misc/EngineVersionComparison.h"

DEFINE_LOG_CATEGORY(LogTetherSimulation);

#if UE_BUILD_DEBUG
#define TETHER_SIMULATION_DEBUG_CHECKS 1
#endif

static TAutoConsoleVariable<int32> CVarDebugSubstep(
	TEXT("Tether.DebugSubstep"),
	-1,
	TEXT("Defines a substep number in the simulation for which to dump extremely verbose debugging information."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarDebugParticle(
    TEXT("Tether.DebugParticle"),
    -1,
    TEXT("Defines a particle unique ID in the simulation for which to dump extremely verbose debugging information."),
    ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarTruncateHits(
	TEXT("Tether.TruncateHits"),
	1,
	TEXT(""),
	ECVF_RenderThreadSafe);

FTetherSimulationResultInfo FTetherSimulation::PerformSimulation(FTetherSimulationModel& Model, float SimulationTime, const FTetherSimulationParams& Params, FProgressCancel* Progress)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("FTetherSimulation::PerformSimulation"))
	
	ensure(Params.World.IsValid(false, true) || !Params.SimulationOptions.bEnableCollision);

	Params.SimulationOptions.CheckSelfCollisionOptions();

	FTetherPhysicsUtils::CheckBodyInstances(Params.BodyInstances);

	FTetherSimulationResultInfo ResultInfo;
	FTetherSimulationContext SimulationContext(Model, Params, ResultInfo);

	UE_LOG(LogTetherSimulation, Verbose, TEXT("-- Begin Tether simulation: %s --"), *Params.SimulationName);
	UE_LOG(LogTetherSimulation, Verbose, TEXT("%s: IsInGameThread (synchronous): %i"), *Params.SimulationName, IsInGameThread());
	UE_LOG(LogTetherSimulation, Verbose, TEXT("%s: Initial model state hash: %i"), *Params.SimulationName, GetTypeHash(Model));
	UE_LOG(LogTetherSimulation, Verbose, TEXT("%s: Params hash: %i"), *Params.SimulationName, GetTypeHash(Params));
	if(UE_LOG_ACTIVE(LogTetherSimulation, VeryVerbose))
	{
		FString Output = TEXT("");
		FTetherSimulationParams::StaticStruct()->ExportText(Output, &Params, nullptr, nullptr, (PPF_ExportsNotFullyQualified | PPF_Copy | PPF_Delimited | PPF_IncludeTransient), nullptr);
		UE_LOG(LogTetherSimulation, VeryVerbose, TEXT("%s: Params: %s"), *Params.SimulationName, *Output);
	}
	if(Params.SimulationOptions.ShouldUseSelfCollision())
	{
		UE_LOG(LogTetherSimulation, Verbose, TEXT("%s: Num Bodies: %i"), *Params.SimulationName, Params.BodyInstances.Num());
		UE_LOG(LogTetherSimulation, Verbose, TEXT("%s: Bodies hash: %i"), *Params.SimulationName, GetBodyInstanceHash(Params));
	}
	UE_LOG(LogTetherSimulation, Verbose, TEXT("%s: Num points: %i"), *Params.SimulationName, Model.GetParticleLocations().Num());
	UE_LOG(LogTetherSimulation, Verbose, TEXT("%s: Total length: %f"), *Params.SimulationName, Model.GetLength());

	const float SubstepTime = Params.SimulationOptions.SubstepTime;
	const int32 NumSubsteps = SimulationTime / SubstepTime;
	UE_LOG(LogTetherSimulation, Verbose, TEXT("%s: SimulationTime: %f, SubstepTime: %f, NumSubsteps: %i"), *Params.SimulationName, SimulationTime, SubstepTime, NumSubsteps);
	
	TArray<FTetherProxySimulationSegmentSeries> SegmentsToSimulate = Params.MakeSegmentSeriesToSimulate(Model, true);

	ResultInfo.SimulatedSegments = {};

	// Log segments
	UE_LOG(LogTetherSimulation, Verbose, TEXT("%s: %i segment series:"), *Params.SimulationName, SegmentsToSimulate.Num());
	for (int32 i = 0; i < SegmentsToSimulate.Num(); i++)
	{
		const FTetherSimulationSegmentSeries& Series = SegmentsToSimulate[i];
		TArray<int32> Segments = Series.GetSegmentUniqueIds();
		ResultInfo.SimulatedSegments.Append(Segments);
		UE_LOG(LogTetherSimulation, Verbose, TEXT("%s:     %i: "), *Params.SimulationName, i);
		UE_LOG(LogTetherSimulation, Verbose, TEXT("%s:         %i Segments: "), *Params.SimulationName, Series.GetNumSegments());
		for (int32 SeriesSegmentIndex = 0; SeriesSegmentIndex < Segments.Num(); SeriesSegmentIndex++)
		{
			const FTetherSimulationSegment* Segment = Series.GetSegmentConst(SeriesSegmentIndex);
			UE_LOG(LogTetherSimulation, Verbose, TEXT("%s:             Index: %i"), *Params.SimulationName, Segment->SegmentUniqueId);
			UE_LOG(LogTetherSimulation, Verbose, TEXT("%s:             SimulationTime: %f"), *Params.SimulationName, Segment->SimulationTime);
		}
	}

	const bool bSimulateEntirely = SimulationTime <= 0.f;
	float SimulationTimeRemainder = SimulationTime;
	int32 SegmentSeriesIndex = 0;
	int32 SegmentSubstepNum = 0;

	while(bSimulateEntirely || SimulationTimeRemainder >= SubstepTime)
	{
		if(Progress && Progress->Cancelled())
		{
			return ResultInfo;
		}
		
		if(!SegmentsToSimulate.IsValidIndex(SegmentSeriesIndex))
		{
			// Out of range, which means there are no more segments to simulate
			break;
		}
		
		FTetherProxySimulationSegmentSeries& Segment = SegmentsToSimulate[SegmentSeriesIndex];
		if(Segment.GetSimulatedTime() + SubstepTime > Params.SimulationOptions.SimulationDuration )
		{
			// Time would exceed the current segment, move to next;
			SegmentSeriesIndex++;
			SegmentSubstepNum = 0;
			continue;
		}

		// Simulate the current segment
		PerformSimulationSubstep(SimulationContext, { Segment }, SubstepTime, SegmentSubstepNum);
		SimulationTimeRemainder -= SubstepTime;
		SegmentSubstepNum++;
	}

	for(FTetherSimulationSegmentSeries& Segment : SegmentsToSimulate)
	{
		Segment.SynchronizeConnectingParticles();
	}

	UE_LOG(LogTetherSimulation, Verbose, TEXT("-- End Tether simulation: %s --"), *Params.SimulationName);

	UE_LOG(LogTetherSimulation, Verbose, TEXT("%s: Num collision hits: %i"), *Params.SimulationName, ResultInfo.NumCollisionHits);

	UE_LOG(LogTetherSimulation, Verbose, TEXT("%s: Simulated model state hash: %i"), *Params.SimulationName, GetTypeHash(Model));

	ResultInfo.SimulationTimeRemainder = SimulationTimeRemainder;
	ResultInfo.SimulatedTime = SimulationTime - SimulationTimeRemainder;
	
	return ResultInfo;
}

FString GetDebugParticleString(const FTetherSimulationParticle& Particle)
{
	FString Output = TEXT("");
	FTetherSimulationParticle::StaticStruct()->ExportText(Output, &Particle, nullptr, nullptr, (PPF_ExportsNotFullyQualified | PPF_Copy | PPF_Delimited | PPF_IncludeTransient), nullptr);
	return Output;
}

FString GetDebugParticleString(const FTetherSimulationModel& Model)
{
	const int32 DebugParticleId = CVarDebugParticle.GetValueOnAnyThread();
	if (DebugParticleId >= 0 && DebugParticleId < Model.GetNumParticles(true))
	{
		const FTetherSimulationParticle& DebugParticle = Model.GetParticleConst(DebugParticleId, true);
		return GetDebugParticleString(DebugParticle);
	}
	return FString();
}

void UpdateSelfCollisionBodies(const FTetherSimulationSubstepContext& SubstepContext)
{
	// The BodyProxy may not contain all the segments that will be simulated, but it will contain all of the ones with bodies relevant to this substep, including this segment and previous ones
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Update Particle Bodies"));
	const FTetherProxySimulationSegmentSeries BodyProxy = SubstepContext.SimulationContext.Model.MakeSeriesUpTo(SubstepContext.SegmentsToSimulate.Last().Segments.Last()->SegmentUniqueId);

	// Find the index of the first particle in the segment we are simulating
	ensure(SubstepContext.SegmentsToSimulate.Num() == 1);
	const int32 FirstBodyIndex = BodyProxy.GetStartingParticleIndexForSegment(BodyProxy.IndexOfSegmentUniqueId(SubstepContext.SegmentsToSimulate.Last().GetSegmentConst(0)->SegmentUniqueId));
	const int32 NumBodies = BodyProxy.GetNumParticles();
	check(NumBodies <= SubstepContext.SimulationContext.Params.BodyInstances.Num());
	for (int32 i = FirstBodyIndex; i < NumBodies; i++)
	{
		FBodyInstance* BodyInstance = SubstepContext.SimulationContext.Params.BodyInstances[i];
		check(BodyInstance->IsValidBodyInstance());
		check(FPhysicsInterface::IsValid(BodyInstance->GetPhysicsActorHandle()));
		FTransform Transform = BodyInstance->GetUnrealWorldTransform();
		Transform.SetLocation(BodyProxy.GetParticleConst(i).Position);
		BodyInstance->SetBodyTransform(Transform, ETeleportType::TeleportPhysics);
	}
}

void FTetherSimulation::PerformSimulationSubstep(FTetherSimulationContext& SimulationContext, TArray<FTetherProxySimulationSegmentSeries> SegmentsToSimulate, float SubstepTime, int32 SubstepNum)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("FTetherSimulation::PerformSimulationSubstep"))

	FTetherSimulationModel& Model = SimulationContext.Model;
	const FTetherSimulationParams& Params = SimulationContext.Params;

	FTetherSimulationSubstepContext SubstepContext(SimulationContext, SegmentsToSimulate);
	SubstepContext.SubstepNum = SubstepNum;

	if(SegmentsToSimulate[0].HasAnyParticles())
	{
		UE_LOG(LogTetherSimulation, VeryVerbose, TEXT("Debug Particle, substep start: %s"), *GetDebugParticleString(Model));

		const float SimulatedTime = SegmentsToSimulate[0].GetSimulatedTime();
		const float ConstraintsEaseInTime = Params.SimulationOptions.ConstraintsEaseInTime;
		const float ForceMultiplier = ConstraintsEaseInTime > 0.f ? FMath::Min(SimulatedTime / Params.SimulationOptions.ConstraintsEaseInTime, 1.f) : 1.f;
		UE_LOG(LogTetherSimulation, VeryVerbose, TEXT("%s: Substep %i: SimulatedTime: %f, ForceMultiplier: %f"), *Params.SimulationName, SubstepNum, SimulatedTime, ForceMultiplier);

		for (FTetherProxySimulationSegmentSeries& Segment : SegmentsToSimulate)
		{
			// Solve new position and velocity from external forces
			VerletIntegrateSegment(SubstepContext, Segment, SubstepTime);
		}

		UE_LOG(LogTetherSimulation, VeryVerbose, TEXT("Debug Particle, after integration: %s"), *GetDebugParticleString(Model));

		for (FTetherProxySimulationSegmentSeries& Segment : SegmentsToSimulate)
		{
			// Apply forces between particles
			SolveConstraintsForSegment(SubstepContext, Segment, ForceMultiplier);
		}

		UE_LOG(LogTetherSimulation, VeryVerbose, TEXT("Debug Particle, after constraints: %s"), *GetDebugParticleString(Model));

		for (FTetherProxySimulationSegmentSeries& Segment : SegmentsToSimulate)
		{
			if (Params.SimulationOptions.bEnableCollision)
			{
				PerformCollision(SubstepContext, Segment, ForceMultiplier);
			}
		}

		UE_LOG(LogTetherSimulation, VeryVerbose, TEXT("Debug Particle, after collision: %s"), *GetDebugParticleString(Model));

		// Update bodies for self-collision
		if (Params.SimulationOptions.ShouldUseSelfCollision())
		{
			UpdateSelfCollisionBodies(SubstepContext);
			UE_LOG(LogTetherSimulation, VeryVerbose, TEXT("%s: Substep %i: Bodies hash: %i"), *Params.SimulationName, SubstepNum, GetBodyInstanceHash(Params));
		}

		UE_LOG(LogTetherSimulation, VeryVerbose, TEXT("%s: Substep %i: Model hash: %i"), *Params.SimulationName, SubstepNum, GetTypeHash(Model));
		if (CVarDebugSubstep.GetValueOnAnyThread() == SubstepNum)
		{
			FString Output = TEXT("");
			FTetherSimulationModel::StaticStruct()->ExportText(Output, &Model, nullptr, nullptr, (PPF_ExportsNotFullyQualified | PPF_Copy | PPF_Delimited | PPF_IncludeTransient), nullptr);
			UE_LOG(LogTetherSimulation, VeryVerbose, TEXT("%s"), *Output);
		}
	}

	// Add simulation time, even for empty segments
	for (FTetherProxySimulationSegmentSeries& Segment : SegmentsToSimulate)
	{
		Segment.AddSimulatedTime(SubstepTime);
	}

}

void FTetherSimulation::VerletIntegrateSegment(FTetherSimulationSubstepContext& SubstepContext, FTetherProxySimulationSegmentSeries& Segment, float SubstepTime)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("FTetherSimulation::VerletIntegrateSegment"))

	const FTetherSimulationParams& Params = SubstepContext.SimulationContext.Params;
	
	const int32 NumParticles = Segment.GetNumParticles();
	const float SubstepTimeSqr = SubstepTime * SubstepTime;

	for (int32 ParticleIdx = 0; ParticleIdx < NumParticles; ParticleIdx++)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Integrate Particle"))
		
		FTetherSimulationParticle& Particle = Segment.GetParticle(ParticleIdx);

#ifdef TETHER_SIMULATION_DEBUG_CHECKS
		ensure(!Particle.Position.ContainsNaN());
#endif
		
		if (Particle.bFree)
		{
			// Calc overall force
			const FVector ParticleForce = Params.CableForce;

			// Find velocity
			FVector Velocity = Particle.Position - Particle.OldPosition;

			if(Params.SimulationOptions.Drag > 0.f)
			{
				const float Speed = Velocity.Size();
				const FVector DragForce = -0.5f * Speed * Speed * Params.SimulationOptions.Drag * Velocity.GetSafeNormal();
				Velocity += DragForce;
			}
			
			// Update position
			const FVector NewPosition = Particle.Position + Velocity + (SubstepTimeSqr * ParticleForce);

			Particle.OldPosition = Particle.Position;
			Particle.Position = NewPosition;

			if(ParticleIdx > 0)
			{
				FTetherSimulationParticle& PreviousParticle = Segment.GetParticle(ParticleIdx - 1);
				ensure(Particle.Position != PreviousParticle.Position);
			}
		}

#ifdef TETHER_SIMULATION_DEBUG_CHECKS
		ensure(!Particle.Position.ContainsNaN());
#endif
	}
}

static void SolveDistanceConstraint(FTetherSimulationParticle& ParticleA, FTetherSimulationParticle& ParticleB, float DesiredDistance, float ForceMultiplier)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("FTetherSimulation::SolveDistanceConstraint"))

	ensure(DesiredDistance > KINDA_SMALL_NUMBER);

	if(ParticleA.ParticleUniqueId == CVarDebugParticle.GetValueOnAnyThread())
	{
		UE_LOG(LogTetherSimulation, VeryVerbose, TEXT("Debug Particle, before solve constraint (A): %s"), *GetDebugParticleString(ParticleA));
	}
	else if (ParticleB.ParticleUniqueId == CVarDebugParticle.GetValueOnAnyThread())
	{
		UE_LOG(LogTetherSimulation, VeryVerbose, TEXT("Debug Particle, before solve constraint (B): %s"), *GetDebugParticleString(ParticleB));
	}
	
	ensure(ParticleA.Position != ParticleB.Position);
	
	// Find current vector between particles
	const FVector Delta = ParticleB.Position - ParticleA.Position;

	const float CurrentDistance = Delta.Size();

	ensure(CurrentDistance < BIG_NUMBER);
	
	const float ErrorFactor = (CurrentDistance - DesiredDistance) / CurrentDistance;
	
	ensure(ErrorFactor != 1);

	// Only move free particles to satisfy constraints
	if (ParticleA.bFree && ParticleB.bFree)
	{
		ParticleA.Position += ErrorFactor * 0.5f * Delta * ForceMultiplier;
		ParticleB.Position -= ErrorFactor * 0.5f * Delta * ForceMultiplier;
	}
	else if (ParticleA.bFree)
	{
		ParticleA.Position += ErrorFactor * Delta * ForceMultiplier;
	}
	else if (ParticleB.bFree)
	{
		ParticleB.Position -= ErrorFactor * Delta * ForceMultiplier;
	}

	ensure(ParticleA.Position != ParticleB.Position);

#ifdef TETHER_SIMULATION_DEBUG_CHECKS
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Check NaN"))
		ensure(!ParticleA.Position.ContainsNaN());
		ensure(!ParticleB.Position.ContainsNaN());
	}
#endif
}

FTetherSimulationParticle& GetParticle(FTetherProxySimulationSegmentSeries& Segment, bool bSynthStart, FTetherSimulationParticle& SynthStartParticle, bool bSynthEnd, FTetherSimulationParticle& SynthEndParticle, int32 ParticleIndex)
{
	const int32 TransformedIndex = bSynthStart ? ParticleIndex - 1 : ParticleIndex;
	
	if(TransformedIndex == -1)
	{
		return SynthStartParticle;
	}
	if(TransformedIndex > Segment.GetNumParticles() - 1)
	{
		ensure(bSynthEnd);
		return SynthEndParticle;
	}

	return Segment.GetParticle(TransformedIndex);
}

void FTetherSimulation::SolveConstraintsForSegment(FTetherSimulationSubstepContext& SubstepContext, FTetherProxySimulationSegmentSeries& Segment, float ForceMultiplier)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("FTetherSimulation::SolveConstraintsForSegment"));

	const FTetherSimulationParams& Params = SubstepContext.SimulationContext.Params;

	const int32 StartPointUniqueId =  Segment.GetSegmentConst(0)->SegmentUniqueId;
	const int32 EndPointUniqueId = Segment.GetLastSegmentConst()->SegmentUniqueId + 1;
	
	const bool bFixedStartTangent = Params.SegmentParams.IsValidIndex(StartPointUniqueId) && Params.SegmentParams[StartPointUniqueId].SimulationOptions.ShouldUseSplineTangents();
	const bool bFixedEndTangent = Params.SegmentParams.IsValidIndex(EndPointUniqueId) && Params.SegmentParams[EndPointUniqueId].SimulationOptions.ShouldUseSplineTangents();
	
	const int32 NumParticleSegments = Segment.GetNumParticleSegments() + bFixedStartTangent + bFixedEndTangent;
	const float ParticleSegmentLength = Segment.GetParticleSegmentLength();
	ensure(ParticleSegmentLength > 0);

	FTetherSimulationParticle SynthStartParticle;
	if(bFixedStartTangent)
	{
		FTetherSimulationParticle& First = Segment.GetParticle(0);
		const FVector Tangent = Segment.GetSplineSegmentInfo().StartLeaveTangent;
		ensure(!Tangent.IsZero());
		const FVector Position = First.Position - (Tangent.GetSafeNormal() * ParticleSegmentLength);
		ensure(Position != First.Position);
		SynthStartParticle = FTetherSimulationParticle(false, Position);
	}
	
	FTetherSimulationParticle SynthEndParticle;
	if(bFixedEndTangent)
	{
		FTetherSimulationParticle& Last = Segment.GetParticle(Segment.GetNumParticles() - 1);
		const FVector Tangent = Segment.GetSplineSegmentInfo().EndArriveTangent;
		ensure(!Tangent.IsZero());
		const FVector Position = Last.Position - (Tangent.GetSafeNormal() * -1.f * ParticleSegmentLength);
		ensure(Position != Last.Position);
		SynthEndParticle = FTetherSimulationParticle(false, Position);
	}
	
	// For each iteration..
	for (int32 IterationIdx = 0; IterationIdx < Params.SimulationOptions.StiffnessSolverIterations; IterationIdx++)
	{
		// Solve distance constraint for each segment
		for (int32 SegIdx = 0; SegIdx < NumParticleSegments; SegIdx++)
		{
			FTetherSimulationParticle& ParticleA = GetParticle(Segment, bFixedStartTangent, SynthStartParticle, bFixedEndTangent, SynthEndParticle, SegIdx);
			FTetherSimulationParticle& ParticleB = GetParticle(Segment, bFixedStartTangent, SynthStartParticle, bFixedEndTangent, SynthEndParticle, SegIdx + 1);
			// Solve for this pair of particles
			SolveDistanceConstraint(ParticleA, ParticleB, ParticleSegmentLength, ForceMultiplier);
		}

		// If desired, solve stiffness constraints (distance constraints between every other particle)
		if (Params.SimulationOptions.bEnableStiffness)
		{
			for (int32 SegIdx = 0; SegIdx < NumParticleSegments - 1; SegIdx++)
			{
				FTetherSimulationParticle& ParticleA = GetParticle(Segment, bFixedStartTangent, SynthStartParticle, bFixedEndTangent, SynthEndParticle, SegIdx);
				FTetherSimulationParticle& ParticleB = GetParticle(Segment, bFixedStartTangent, SynthStartParticle, bFixedEndTangent, SynthEndParticle, SegIdx + 2);
				SolveDistanceConstraint(ParticleA, ParticleB, 2.f * ParticleSegmentLength, ForceMultiplier);
			}
		}
	}
}

FHitResult* GetBestHit(const ::FTetherSimulationSubstepContext& SubstepContext, const ::FTetherSimulationParticle& SimulatingParticle, TArray<FHitResult>& Hits, int32 ParticleCableIndex, TWeakObjectPtr<UPrimitiveComponent> Component)
{
	const FVector& ParticleStartPos = SimulatingParticle.OldPosition;
	FHitResult* ClosestHit = nullptr;
	float ClosestDistSquared = BIG_NUMBER;
	for(FHitResult& Hit : Hits)
	{
#if UE_VERSION_OLDER_THAN(5,0,0)
		const bool bWeakPtrValid = Hit.Actor.IsValid(false, true);
		const AActor* Actor = bWeakPtrValid ? Hit.GetActor() : nullptr;
#else
		const bool bValidHitObject = Hit.HasValidHitObjectHandle();
		const AActor* Actor = bValidHitObject ? Hit.GetActor() : nullptr;
#endif
		const bool bObjectValid = IsValid(Actor);
		if(bObjectValid && Actor->IsA<ATriggerBase>())
		{
			// Ignore trigger actors
			continue;
		}
		
		if(Component.IsValid(false, true) && Component.Get() == Hit.GetComponent())
		{
			if(Hit.Item < 0)
			{
				continue;
			}
			
			// Hit other particle on cable
			if (Hit.Item >= ParticleCableIndex - 2 && Hit.Item <= ParticleCableIndex + 2)
			{
				// Ignore particles that are too close
				continue;
			}

			const uint32 OtherParticleSegmentIndex = SubstepContext.SimulationContext.Model.GetSegmentWithParticle(Hit.Item)->SegmentUniqueId;
			if(OtherParticleSegmentIndex > SubstepContext.SimulationContext.Model.GetSegmentWithParticle(SimulatingParticle.ParticleUniqueId, true)->SegmentUniqueId)
			{
				// Hit other particle in a future segment, ignore
				continue;
			}
		}
		const float DistSquared = FVector::DistSquared(ParticleStartPos, Hit.ImpactPoint);
		if(!ClosestHit || DistSquared < ClosestDistSquared)
		{
			ClosestHit = &Hit;
			ClosestDistSquared = DistSquared;
		}
	}
	return ClosestHit;
}

double TruncDoubleOneDecimal(double Val)
{
	// Truncating to 1 decimal place seems to fix all floating point non-determinism
	// (2 decimal places can still cause issues)
#if UE_VERSION_OLDER_THAN(4,25,0)
	return trunc(Val * 10.) / 10.;
#else
	return FMath::TruncToDouble(Val * 10.) / 10.;
#endif
}

FVector TruncVector(FVector Val)
{
	return FVector(TruncDoubleOneDecimal(Val.X), TruncDoubleOneDecimal(Val.Y), TruncDoubleOneDecimal(Val.Z));
}

// The hit results from PhysX appear to be subject to non-determinism in the form of floating point imprecision
// We manually truncate the hit results so that the imprecision is smoothed out and the result is made deterministic again
void TruncHit(FHitResult& Hit)
{
	if(CVarTruncateHits.GetValueOnAnyThread() > 0)
	{
		Hit.Normal = TruncVector(Hit.Normal);
		Hit.Location = TruncVector(Hit.Location);
		Hit.PenetrationDepth = TruncDoubleOneDecimal(Hit.PenetrationDepth);
	}
}

void ResolveHit(FTetherSimulationSubstepContext& SubstepContext, FTetherSimulationParticle& Particle, FHitResult& HitResult, float CollisionFriction, float ForceMultiplier)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Resolve Particle Collision Hit"));

	const FTetherSimulationParams& Params = SubstepContext.SimulationContext.Params;
	FTetherSimulationResultInfo& ResultInfo = SubstepContext.SimulationContext.ResultInfo;
	FTetherSimulationModel& Model = SubstepContext.SimulationContext.Model;
	
	ResultInfo.HitComponents.AddUnique(HitResult.Component);
	ResultInfo.NumCollisionHits++;

	const float PenetrationDepth = HitResult.PenetrationDepth; //TruncFloat(HitResult.PenetrationDepth);
	const FVector Normal = HitResult.Normal; // TruncVector(HitResult.Normal);
	const FVector Location = HitResult.Location; //TruncVector(HitResult.Location);
	
	if (HitResult.bStartPenetrating)
	{
		Particle.Position += (Normal * PenetrationDepth) * ForceMultiplier;
	}
	else
	{
		Particle.Position = Location;
	}

	bool bResolved = false;
	const bool bIsSelfCollision = HitResult.Component.IsValid(false, true) && Params.Component.IsValid(false, true) && HitResult.Component.Get() == Params.Component.Get();
	if(bIsSelfCollision)
	{
		check(HitResult.Item >= 0);
		const int32 ThisParticleSegmentIndex = Model.GetSegmentWithParticle(Particle.ParticleUniqueId, true)->SegmentUniqueId;
		
		FTetherSimulationParticle& OtherParticle = Model.GetParticle(HitResult.Item);
		const int32 OtherParticleSegmentIndex = Model.GetSegmentWithParticle(HitResult.Item)->SegmentUniqueId;

		if(ThisParticleSegmentIndex == OtherParticleSegmentIndex)
		{
			// Two-way resolution
			
			// We can only do a two-way resolution if both particles are in the same segment
			const FVector RelativeVelocity =  (Particle.Position - Particle.OldPosition) - (OtherParticle.Position - OtherParticle.OldPosition);
			const float VelocityAlongNormal = RelativeVelocity | Normal;
			if (VelocityAlongNormal > 0)
			{
				return;
			}
			const FVector HalfImpulse = VelocityAlongNormal * Normal * 0.5f;
			Particle.OldPosition += HalfImpulse;
			OtherParticle.OldPosition -= HalfImpulse;

			if(IsInGameThread() && Particle.ParticleUniqueId == CVarDebugParticle.GetValueOnAnyThread())
			{
				::DrawDebugLine(SubstepContext.SimulationContext.Params.World.Get(), Particle.Position, Particle.Position + HalfImpulse * 200.f, FColor::Cyan, false, 2, 0, 1);
			}

			// Apply friction in plane of collision if desired
			//if (CollisionFriction > KINDA_SMALL_NUMBER)
			//{
			//	// Find component in plane
			//	const FVector PlaneDelta = RelativeVelocity - (VelocityAlongNormal * Normal);

			//	// Scale plane delta  by 'friction'
			//	const FVector ScaledPlaneDelta = PlaneDelta * CollisionFriction;

			//	// Apply delta to old position reduce implied velocity in collision plane
			//	Particle.OldPosition += ScaledPlaneDelta;
			//	OtherParticle.OldPosition -= ScaledPlaneDelta;
			//}
			
			bResolved = true;
		}
	}

	if(!bResolved)
	{
		// One-way resolution
		
		// Find new velocity, after fixing collision
		const FVector Velocity = Particle.Position - Particle.OldPosition;
		// Find component in normal
		const float VelocityAlongNormal = Velocity | Normal;

		// Zero out any positive separation velocity, basically zero restitution
		const FVector Impulse = VelocityAlongNormal * Normal;
		Particle.OldPosition += Impulse;

		if (IsInGameThread() && Particle.ParticleUniqueId == CVarDebugParticle.GetValueOnAnyThread())
		{
			::DrawDebugLine(SubstepContext.SimulationContext.Params.World.Get(), Particle.Position, Particle.Position + Impulse * 100.f, FColor::White, false, 2, 0, 1);
		}

		// Apply friction in plane of collision if desired
		if (CollisionFriction > KINDA_SMALL_NUMBER)
		{
			// Find component in plane
			const FVector PlaneDelta = Velocity - (VelocityAlongNormal * Normal);
			
			// Scale plane delta  by 'friction'
			const FVector ScaledPlaneDelta = PlaneDelta * CollisionFriction;

			// Apply delta to old position reduce implied velocity in collision plane
			Particle.OldPosition += ScaledPlaneDelta;
		}
		bResolved = true;
	}

}

void FTetherSimulation::PerformCollision(FTetherSimulationSubstepContext& SubstepContext, FTetherProxySimulationSegmentSeries& SimulatingSegmentSeries, float ForceMultiplier)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("FTetherSimulation::PerformCollision"));

	FTetherSimulationModel& Model = SubstepContext.SimulationContext.Model;
	const FTetherSimulationParams& Params = SubstepContext.SimulationContext.Params;
	
	if(!Params.World.IsValid(false, true))
	{
		return;
	}
	UWorld* World = Params.World.Get();

	UE_LOG(LogTetherSimulation, VeryVerbose, TEXT("%s: Substep %i: PhysicsSceneHash: %i"), *Params.SimulationName, SubstepContext.SubstepNum, FTetherPhysicsUtils::HashPhyiscsBodies(World));

	const bool bDetailedSubstepDebug = CVarDebugSubstep.GetValueOnAnyThread() == SubstepContext.SubstepNum;
	const int32 DetailedDebugParticle = CVarDebugParticle.GetValueOnAnyThread();

	const int32 StartingParticleCableIndex = Model.GetStartingParticleIndexForSegment(SimulatingSegmentSeries.GetFirstSegmentUniqueId());

	float CableWidth = Params.CollisionWidth;
	float CollisionFriction = Params.SimulationOptions.CollisionFriction;

	// Get collision settings from component
	const FCollisionQueryParams& QueryParams = Params.CollisionQueryParams;

	FCollisionProfileName TraceProfile = Params.SimulationOptions.CollisionProfile;
	ECollisionChannel TraceChannel = ECC_PhysicsBody;
	FCollisionResponseParams ResponseParams = FCollisionResponseParams();
	UCollisionProfile::GetChannelAndResponseParams(TraceProfile.Name, TraceChannel, ResponseParams);

	FCollisionShape CollisionShape = FCollisionShape::MakeSphere(0.5f * CableWidth);

	const int32 NumParticles = SimulatingSegmentSeries.GetNumParticles();

	// Iterate over each particle
	for (int32 ParticleIdx = 0; ParticleIdx < NumParticles; ParticleIdx++)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Perform Particle Collision"))
		FTetherSimulationParticle& Particle = SimulatingSegmentSeries.GetParticle(ParticleIdx);
		// If particle is free
		if (Particle.bFree)
		{
			const int32 ParticleCableIndex = StartingParticleCableIndex + ParticleIdx;
			
			// Do sphere sweep
			TArray<FHitResult> Result;

			// Note: Sweep single in PhysX does not appear to be deterministic. If the swept shape is intersecting multiple bodies at the start of the simulation, it seems that the hit result that is returned is random
			// So we do a sweep multi and manually choose the result deterministically
			bool bHit = World->SweepMultiByChannel(Result, Particle.OldPosition, Particle.Position, FQuat::Identity, TraceChannel, CollisionShape, QueryParams, ResponseParams);
			// If we got a hit, resolve it
			if (bHit)
			{
				FHitResult* Hit = GetBestHit(SubstepContext, Particle, Result, ParticleCableIndex, Params.Component);
				if(Hit)
				{
					TruncHit(*Hit);
				}
				if(bDetailedSubstepDebug || DetailedDebugParticle == Particle.ParticleUniqueId)
				{
					// Detailed logging
					UE_LOG(LogTetherSimulation, VeryVerbose, TEXT("%s: Substep %i: Particle %i: ParticleUniqueId %i"), *Params.SimulationName, SubstepContext.SubstepNum, ParticleIdx, Particle.ParticleUniqueId);
					UE_LOG(LogTetherSimulation, VeryVerbose, TEXT("%s: Start: %s, End: %s"), *Params.SimulationName, *Particle.OldPosition.ToString(), *Particle.Position.ToString());

					if(UE_LOG_ACTIVE(LogTetherSimulation, VeryVerbose))
					{
						Algo::Sort(Result, [](FHitResult& A, FHitResult& B)
                        {
                             return A.Item < B.Item;
                        });
						for( int32 i = 0; i < Result.Num(); i++)
						{
							TruncHit(Result[i]);
						}
						for(int32 i = 0; i< Result.Num(); i++)
						{
							FString Output = TEXT("");
							FHitResult::StaticStruct()->ExportText(Output, &Result[i], nullptr, nullptr, (PPF_ExportsNotFullyQualified | PPF_Copy | PPF_Delimited | PPF_IncludeTransient), nullptr);
							UE_LOG(LogTetherSimulation, VeryVerbose, TEXT("%s: Substep %i: Particle %i: Hit %i: %s"), *Params.SimulationName, SubstepContext.SubstepNum, ParticleIdx, i, *Output);
						}
					}

				}

				if(Hit)
				{
					ResolveHit(SubstepContext, Particle, *Hit, CollisionFriction, ForceMultiplier);
				}

				if (bDetailedSubstepDebug)
				{
					FString Output = TEXT("");
					FTetherSimulationParticle::StaticStruct()->ExportText(Output, &Particle, nullptr, nullptr, (PPF_ExportsNotFullyQualified | PPF_Copy | PPF_Delimited | PPF_IncludeTransient), nullptr);
					UE_LOG(LogTetherSimulation, VeryVerbose, TEXT("%s: Particle: %s"), *Params.SimulationName, *Output);
				}

			}
		}
#ifdef TETHER_SIMULATION_DEBUG_CHECKS
		ensure(!Particle.Position.ContainsNaN());
#endif
	}

}
