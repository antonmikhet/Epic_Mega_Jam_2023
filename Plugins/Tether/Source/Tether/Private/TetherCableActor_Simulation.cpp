// Copyright Sam Bonifacio 2021. All Rights Reserved.

#include "CableSplineUtils.h"
#include "EngineUtils.h"
#include "TetherCableActor.h"
#include "TetherDebugDrawHelpers.h"
#include "TetherLogs.h"
#include "Mesh/TetherCableMeshComponent.h"
#include "Simulation/TetherSimulation.h"
#if WITH_EDITOR
#include "Editor.h"
#endif

bool ATetherCableActor::CanBeModified() const
{
#if WITH_EDITOR
	// Don't allow simulation while running game
	return !IsRunningGame()
	&& !IsRunningCommandlet()
	&& IsValid(GetWorld())
	&& !GetWorld()->IsGameWorld();
#else
	return false;
#endif
}

#if WITH_EDITOR

bool ValidateTangents(FTetherSimulationModel& Model, TArray<UTetherPointSegmentDefinition*> PointSegmentDefinitions)
{
	for(int32 i=0 ; i< Model.GetNumSegments(); i++)
	{
		UTetherPointSegmentDefinition* StartPoint = PointSegmentDefinitions[i];
		UTetherPointSegmentDefinition* EndPoint = PointSegmentDefinitions[i + 1];
		const FTetherSimulationSegment* Segment = Model.GetSegmentConst(i);

		// If the segment should be using tangents on either side, make sure they're not zero
		if(StartPoint->SimulationOptions.ShouldUseSplineTangents())
		{
			if(Segment->SplineSegmentInfo.StartLeaveTangent.IsZero())
			{
				return false;
			}
		}
		if(EndPoint->SimulationOptions.ShouldUseSplineTangents())
		{
			if(Segment->SplineSegmentInfo.EndArriveTangent.IsZero())
			{
				return false;
			}
		}
	}
	return true;
}

FVector ATetherCableActor::GetPointLocation(int32 PointIndex) const
{
	return GuideSpline->GetLocationAtSplinePoint(PointIndex, ESplineCoordinateSpace::World);
}

void ATetherCableActor::SetPointLocation(int32 PointIndex, FVector WorldLocation, bool bSimulateIfModified)
{
	GuideSpline->SetLocationAtSplinePoint(PointIndex, WorldLocation, ESplineCoordinateSpace::World);
	if(bSimulateIfModified)
	{
		UE_LOG(LogTetherCable, VeryVerbose, TEXT("%s: SetPointLocation -> UpdateAndRebuildModifiedSegments"), *GetHumanReadableName());
		UpdateAndRebuildModifiedSegments();
	}
}

void ATetherCableActor::SetEndLocation(FVector EndLocation)
{
	const int32 Index = GetNumGuideSplinePoints() - 1;

	SetPointLocation(Index, EndLocation);
}

bool ATetherCableActor::UpdatePreviewPoint(int32 Point, FVector WorldLocation, bool bSimulateIfModified)
{
	if(!ensure(Point < GetNumGuideSplinePoints()))
	{
		return false;
	}
	
	const FVector CurrentEndLocation = GuideSpline->GetLocationAtSplinePoint(Point, ESplineCoordinateSpace::World);
	if (CurrentEndLocation.Equals(WorldLocation, 1.f))
	{
		return false;
	}

	UE_LOG(LogTetherCable, Verbose, TEXT("%s: Updating preview point"), *GetHumanReadableName());

	SetPointLocation(Point, WorldLocation, bSimulateIfModified);
	
	return true;
}

void ATetherCableActor::AddSegmentSlack(int32 SegmentIndex, float DeltaSlack, bool bSimulateIfModified)
{	
	if(!ensure(SegmentIndex >= 0 && SegmentIndex < GetNumGuideSplinePoints()))
	{
		return;
	}
	
	UTetherPointSegmentDefinition* SegmentDef = GetPointSegmentDefinitions()[SegmentIndex];
	
	const float NewSlack = SegmentDef->Slack + DeltaSlack;
	UE_LOG(LogTetherCable, Verbose, TEXT("%s: Slack: %f"), *GetHumanReadableName(), NewSlack);
	SegmentDef->Slack = NewSlack;
	
	InvalidateSegments({ SegmentIndex });
	if(bSimulateIfModified)
	{
		ResimulateInvalidatedSegments();
	}
}

void ATetherCableActor::AddGuideSplinePoint(int32 Index, FVector EndLocation)
{
	if(!ensure(IsValid(GuideSpline)))
	{
		return;
	}
	
	const int32 NumExistingPoints = GetNumGuideSplinePoints();
	if(!ensure(Index <= NumExistingPoints))
	{
		return;
	}

	GuideSpline->Modify();
	GuideSpline->AddSplinePointAtIndex(EndLocation, Index, ESplineCoordinateSpace::World, true);
	GuideSpline->SetSplinePointType(Index, ESplinePointType::Linear, true);

	UE_LOG(LogTetherCable, VeryVerbose, TEXT("%s: AddGuideSplinePoint -> UpdateAndRebuildModifiedSegments"), *GetHumanReadableName());
	UpdateAndRebuildModifiedSegments();
}

void ATetherCableActor::RemovePreviewEndpoint()
{
	const int32 Index = GetNumGuideSplinePoints() - 1;
	GuideSpline->RemoveSplinePoint(Index, true);

	UE_LOG(LogTetherCable, VeryVerbose, TEXT("%s: RemovePreviewEndpoint -> UpdateAndRebuildModifiedSegments"), *GetHumanReadableName());
	UpdateAndRebuildModifiedSegments();
}

int32 ATetherCableActor::GetNumGuideSplinePoints() const
{
	return GuideSpline->SplineCurves.Position.Points.Num();
}

int32 ATetherCableActor::GetNumGuideSplineSegments() const
{
	return GetNumGuideSplinePoints() - 1;
}

TArray<UTetherPointSegmentDefinition*> ATetherCableActor::GetPointSegmentDefinitions() const
{
	return GetGuideSplineComponent()->GetPointSegmentDefinitions();
}

bool ATetherCableActor::ShouldSimulateBefore(const ATetherCableActor* OtherCable) const
{
	if(TimeCreated != OtherCable->TimeCreated)
	{
		// Simulate the cable that was created first
		return TimeCreated < OtherCable->TimeCreated;
	}

	// If two cables were created at the exact same time, tie-break with the hashed actor name (which is unique) so at least the result is deterministic even though it's effectively random
	return GetTypeHash(GetName()) < GetTypeHash(OtherCable->GetName());
}

void ATetherCableActor::UpdateAndRebuildModifiedSegments(bool bSynchronous, EMeshBuildInstruction BuildMesh, bool bSimulateIfModified)
{
	checkNoRecursion();

	ensure(CanBeModified());

	// This is no-op safe and can be called very frequently, so be careful about putting logs here that might be spammed
	
	if(bLockCurrentState)
	{
		// Make sure we can never rebuild while state is locked
		return;
	}

	// Build mesh if guide spline was modified
	if(bGuideSplineModified && BuildMesh == DoNotBuild)
	{
		UE_LOG(LogTetherCable, Verbose, TEXT("%s: UpdateAndRebuildModifiedSegments: Guide spline modified, rebuild mesh"), *GetHumanReadableName());
		BuildMesh = BuildIfModified;
	}

	if(IsAnySegmentInvalidated())
	{
		UE_LOG(LogTetherCable, Verbose, TEXT("%s: UpdateAndRebuildModifiedSegments: Segments were already invalidated"), *GetHumanReadableName());
	}

	const bool bSegmentsRemoved = PendingRemovedGuideSplinePoints.Num() > 0;
	if(bSegmentsRemoved)
	{
		UE_LOG(LogTetherCable, Verbose, TEXT("%s: %i segments, %i particles"), *GetHumanReadableName(), ActiveSimulationModel.GetNumSegments(), ActiveSimulationModel.GetNumParticles());
		// Remove segments in the order of the spline points being removed
		for(const int32 SegmentIndex : PendingRemovedGuideSplinePoints)
		{
			ensure(SegmentIndex >= 0 && SegmentIndex <= ActiveSimulationModel.Segments.Num());

			if(SegmentIndex == ActiveSimulationModel.Segments.Num())
			{
				// The last point was removed, so remove the last segment
				UE_LOG(LogTetherCable, Verbose, TEXT("%s: Last guide spline point removed, removing simulation segment"), *GetHumanReadableName());
				ActiveSimulationModel.Segments.RemoveAt(SegmentIndex - 1);
			}
			else
			{
				UE_LOG(LogTetherCable, Verbose, TEXT("%s: Guide spline point %i removed, removing simulation segment"), *GetHumanReadableName(), SegmentIndex);
				ActiveSimulationModel.Segments.RemoveAt(SegmentIndex);
			}
		}

		// Update unique IDs of segments as the indices may have changed due to segment removal
		ActiveSimulationModel.UpdateSegmentUniqueIds();

		InvalidateAllSegments();

		// If segments were removed, update mesh now, so that the mesh for the segment is removed immediately
		UE_LOG(LogTetherCable, Verbose, TEXT("%s: Segments removed, so update and show dynamic mesh"), *GetHumanReadableName());
		UE_LOG(LogTetherCable, Verbose, TEXT("%s: %i segments, %i particles"), *GetHumanReadableName(), ActiveSimulationModel.GetNumSegments(), ActiveSimulationModel.GetNumParticles());

		bool bBuildStarted = false;
		
		if(ActiveSimulationModel.HasAnyParticles())
		{
			// If we have any particles, build the static mesh
			UE_LOG(LogTetherCable, VeryVerbose, TEXT("%s: UpdateAndRebuildModifiedSegments -> QueueBuildAndShowStaticMesh"), *GetHumanReadableName());
			bBuildStarted |= QueueBuildAndShowStaticMesh(false, bSynchronous);
		}

		if(!bBuildStarted)
		{
			// If didn't build mesh, update dynamic mesh
			UpdateMeshVisibilities(false, false);
		}
	}

	// Note: Segment removals have to be processed before updating simulation segment num, otherwise the removals will be processed incorrectly
	// Do this before invalidating modified segments
	const bool bModifiedSegmentNum = UpdateSimulationSegmentNum();

	if(!bSegmentsRemoved)
	{
		// Invalidate segments that were modified
		// Don't check this if segments were removed, because they will already be invalidated
		const TArray<int32> ModifiedSegments = FindModifiedSegments();
		if(ModifiedSegments.Num() > 0)
		{
			UE_LOG(LogTetherCable, VeryVerbose, TEXT("%s: %i segments were modified, invalidating"), *GetHumanReadableName(), ModifiedSegments.Num());
			InvalidateSegments(ModifiedSegments);
		}
	}

	const bool bAnySegmentInvalidated = IsAnySegmentInvalidated();
	
	const bool bStateInvalidated = bModifiedSegmentNum || bAnySegmentInvalidated;
	const bool bShouldTrySimulate = bStateInvalidated && bSimulateIfModified;
	if(bShouldTrySimulate)
	{
		UE_LOG(LogTetherCable, VeryVerbose, TEXT("%s: UpdateAndRebuildModifiedSegments bShouldTrySimulate is true"), *GetHumanReadableName());
		UE_LOG(LogTetherCable, VeryVerbose, TEXT("%s: bModifiedSegmentNum: %i, bAnySegmentInvalidated: %i, bSimulateIfModified %i"), *GetHumanReadableName(), (int32)bModifiedSegmentNum, (int32)bAnySegmentInvalidated, (int32)bSimulateIfModified);
		bSimulationOutdated = false;
		UE_LOG(LogTetherCable, VeryVerbose, TEXT("%s: UpdateAndRebuildModifiedSegments -> ResimulateInvalidatedSegments, bSynchronous: %i, bBuildMesh: %i"), *GetHumanReadableName(), (int32)bSynchronous, (int32) BuildMesh);
		ResimulateInvalidatedSegments(bSynchronous, BuildMesh);
	}

	if(!bShouldTrySimulate)
	{

		if(BuildMesh >= BuildIfModified && ActiveSimulationModel.HasAnyParticles())
		{
			// Caller wanted to build mesh, but we didn't end up simulating, queue up the build
			UE_LOG(LogTetherCable, VeryVerbose, TEXT("%s: UpdateAndRebuildModifiedSegments: Caller wanted to build mesh, but didn't simulate -> QueueBuildAndShowStaticMesh"), *GetHumanReadableName());
			QueueBuildAndShowStaticMesh(false, bSynchronous);
		}

	}

	// Reset pending guide-spline modifications
	bGuideSplineModified = false;
	PendingRemovedGuideSplinePoints.Reset();

}

void ATetherCableActor::InvalidateAndResimulate(bool bSynchronous, EMeshBuildInstruction BuildMesh)
{
	UE_LOG(LogTetherCable, Verbose, TEXT("%s: InvalidateAndResimulate, bSynchronous: %i, bBuildMesh: %i"), *GetHumanReadableName(), (int32)bSynchronous, (int32)BuildMesh);

	ensure(CanBeModified());
	
	// Invalidate segments so that we can re-sim with new properties
	InvalidateAllSegments();

	if(bLockCurrentState)
	{
		UE_LOG(LogTetherCable, Verbose, TEXT("%s: bLockCurrentState is true, abort InvalidateAndResimulate"), *GetHumanReadableName());
		return;
	}

	UE_LOG(LogTetherCable, VeryVerbose, TEXT("%s: InvalidateAndResimulate -> UpdateAndRebuildModifiedSegments"), *GetHumanReadableName());
	UpdateAndRebuildModifiedSegments(bSynchronous, BuildMesh);

	// The following should only occur if the simulation state is valid (has particles)
	const bool bSimulationHasParticles = ActiveSimulationModel.HasAnyParticles();

	// Set mesh visibilities if state is NOT locked
	// This may show either the static if it is up to date, or the updated dynamic mesh if any mesh params were modified
	if (bSimulationHasParticles)
	{
		UpdateMeshVisibilitiesConditional(true);
	}
}

int32 ATetherCableActor::GetNumSimulationSegments() const
{
	return ActiveSimulationModel.Segments.Num();
}

TArray<FTetherSimulationSegment>& ATetherCableActor::GetSimulationSegments()
{
	return ActiveSimulationModel.Segments;
}

const TArray<FTetherSimulationSegment>& ATetherCableActor::GetSimulationSegmentsConst() const
{
	return ActiveSimulationModel.Segments;
}

void ATetherCableActor::SetRealtimeSimulating(bool bShouldRealtimeSimulate)
{
	if(bRealtimeSimulating != bShouldRealtimeSimulate)
	{
		bRealtimeSimulating = bShouldRealtimeSimulate;
		if (bRealtimeSimulating)
		{
			RealtimeSimulationTimeRemainder = 0.f;
			CancelAsyncSimulation();
		}
		InvalidateAndResimulate(false, BuildIfModified);
	}
}

void ATetherCableActor::CancelAsyncSimulation()
{
	if(CurrentSimulationTask)
	{
		CurrentSimulationTask->CancelAndDelete();
		CurrentSimulationTask = nullptr;
		UE_LOG(LogTetherCable, Verbose, TEXT("%s: Cancelled async simulation task, will be deleted"), *GetHumanReadableName());
		CurrentSegmentsRunningAsyncSimulation = {};
		bSimulationOutdated = true;
	}
}

bool ATetherCableActor::UpdateSimulationSegmentNum()
{
	if (GetNumSimulationSegments() != GetNumGuideSplineSegments())
	{
		UE_LOG(LogTetherCable, VeryVerbose, TEXT("%s: Num Simulation Segments %i did not match Num Guide Spline Segments %%i, updating"), *GetHumanReadableName(), GetNumSimulationSegments(), GetNumGuideSplineSegments());
		ActiveSimulationModel.UpdateNumSegments(GetNumGuideSplineSegments());
		return true;
	}
	return false;
}

TArray<int32> ATetherCableActor::FindModifiedSegments() const
{
	TArray<int32> Result;
	for (int i = 0; i < GetNumSimulationSegments(); i++)
	{
		const FTetherSimulationSegment& Segment = ActiveSimulationModel.Segments[i];
		const bool bStartTangent = GetPointSegmentDefinitions()[i]->SimulationOptions.bUseSplineTangents;
		const bool bEndTangent = GetPointSegmentDefinitions()[i + 1]->SimulationOptions.bUseSplineTangents;
		const FSplineSegmentInfo GuideSplineSegmentInfo = FCableSplineUtils::SegmentInfoFromSplineComponent(GuideSpline, i, GetCableRootTransform());
		if(!Segment.SplineSegmentInfo.Equals(GuideSplineSegmentInfo, KINDA_SMALL_NUMBER, bStartTangent, bEndTangent))
		{
			UE_LOG(LogTetherCable, VeryVerbose, TEXT("%s: Spline info modified, segment %i"), *GetHumanReadableName(), i);
			UE_LOG(LogTetherCable, VeryVerbose, TEXT("%s: Segment: %s"), *GetHumanReadableName(), *Segment.SplineSegmentInfo.ToString());
			UE_LOG(LogTetherCable, VeryVerbose, TEXT("%s: Guide: %s"), *GetHumanReadableName(), *GuideSplineSegmentInfo.ToString());
			Result.Add(i);
		}
	}
	return Result;
}

bool ATetherCableActor::InvalidateSegment(FTetherSimulationSegment& Segment)
{
	if (!Segment.IsInvalidated())
	{
		UE_LOG(LogTetherCable, VeryVerbose, TEXT("%s: Invalidated Segment %i"), *GetHumanReadableName(), Segment.SegmentUniqueId);
		Segment.SetInvalidated(true);
		return true;
	}
	return false;
}

bool ATetherCableActor::InvalidateSegments(TArray<int32> SegmentIndices)
{
	bool bAnyInvalidated = false;
	
	for(int32 Index : SegmentIndices)
	{
		if(ActiveSimulationModel.Segments.IsValidIndex(Index))
		{
			if (InvalidateSegment(ActiveSimulationModel.Segments[Index]))
			{
				bAnyInvalidated = true;
			}
		}
	}

	return bAnyInvalidated;
}

void ATetherCableActor::InvalidateAllSegments()
{
	for(FTetherSimulationSegment& Segment : ActiveSimulationModel.Segments)
	{
		InvalidateSegment(Segment);
	}
}

bool ATetherCableActor::IsAnySegmentInvalidated() const
{
	for(const FTetherSimulationSegment& Segment : ActiveSimulationModel.Segments)
	{
		if(Segment.IsInvalidated())
		{
			return true;
		}
	}
	return false;
}

void ATetherCableActor::ResimulateInvalidatedSegments(bool bSynchronous, EMeshBuildInstruction BuildMesh)
{
	checkNoRecursion();
	
	if(bLockCurrentState)
	{
		return;
	}
	
	if(!IsAnySegmentInvalidated())
	{
		return;
	}

	UE_LOG(LogTetherCable, VeryVerbose, TEXT("%s: ResimulateInvalidatedSegments, bSynchronous: %i, bBuildMesh: %i"), *GetHumanReadableName(), (int32) bSynchronous, (int32) BuildMesh);
	
	UpdateInvalidSimulationSegmentsSplineInfo(ActiveSimulationModel);

	UE_LOG(LogTetherCable, VeryVerbose, TEXT("%s: ResimulateInvalidatedSegments: Updated active model segments, hash: %i"), *GetHumanReadableName(), GetTypeHash(ActiveSimulationModel));

	if (bRealtimeSimulating)
	{
		// If we're simulating in realtime, reset the state but don't resimulate
		ResetInvalidatedSegmentsSimulationState(ActiveSimulationModel);
		return;
	}
	
	if(IsRunningAsyncSimulation())
	{
		if(bSynchronous)
		{
			// If starting a synchronous simulation, cancel current async one
			CancelAsyncSimulation();
		}
		else
		{
			// If we're already running an async simulation, don't start another async one
			// When the current one returns it will check if anything else was invalidated and start another if it needs to

			UE_LOG(LogTetherCable, Verbose, TEXT("%s: ResimulateInvalidatedSegments: Already running simulation, so abort this one"), *GetHumanReadableName());
		
			if(BuildMesh >= BuildIfModified)
			{
				UE_LOG(LogTetherCable, Verbose, TEXT("%s: ResimulateInvalidatedSegments: bBuildMesh was true, so set bBuildAfterNextSimulation"), *GetHumanReadableName());
				bBuildAfterNextSimulation = true;
			}

			return;
		}

	}
	
	UE_LOG(LogTetherCable, VeryVerbose, TEXT("%s: ResimulateInvalidatedSegments: Rebuilding working model particles"), *GetHumanReadableName());
	
	// Regenerate and asynchronously pre-warm a copy of active model without touching the original
	// We need to rebuild particles for the upcoming simulation, but keep the existing particles for the currently active simulation until the new simulation returns
	FTetherSimulationModel WorkingModel = ActiveSimulationModel;
	//::DrawDebugPoint(GetWorld(), WorkingModel.SimulationBaseWorldTransform.GetLocation(), 60.f, FColor::Red, false, 2.f, 1);
	ResetInvalidatedSegmentsSimulationState(WorkingModel);

	UE_LOG(LogTetherCable, Verbose, TEXT("%s: ResimulateInvalidatedSegments: Updated working model particles, num: %i, hash: %i"), *GetHumanReadableName(), WorkingModel.GetNumParticles(true), GetTypeHash(WorkingModel));

	if(!WorkingModel.HasAnyParticles())
	{
		// Abort since there is nothing to simulate
		return;
	}
	
	// Mark all segments of active simulation as valid since we are about to simulate them
	// We do this on the active simulation model rather than the working model so that it can be invalidated again while it's async simulating
	bool bUninvalidated = false;
	for (FTetherSimulationSegment& Segment : ActiveSimulationModel.Segments)
	{
		if(Segment.IsInvalidated())
		{
			UE_LOG(LogTetherCable, VeryVerbose, TEXT("%s: ResimulateInvalidatedSegments: Uninvalidate segment %i"), *GetHumanReadableName(), Segment.SegmentUniqueId);
			Segment.SetInvalidated(false);
			bUninvalidated = true;
		}
	}

	if(!bUninvalidated)
	{
		UE_LOG(LogTetherCable, VeryVerbose, TEXT("%s: ResimulateInvalidatedSegments: No segments were marked as invalidated"), *GetHumanReadableName());
	}

	// Pass SimulationDuration as zero to flag full simulation of the model
	constexpr float SimulationDuration = 0.f;

	if(bBuildAfterNextSimulation)
	{
		if(BuildMesh < BuildIfModified)
		{
			UE_LOG(LogTetherCable, Verbose, TEXT("%s: ResimulateInvalidatedSegments: bBuildAfterNextSimulation was true, so set BuildIfModified"), *GetHumanReadableName());
			BuildMesh = BuildIfModified;
		}
		bBuildAfterNextSimulation = false;
	}
	
	// Run full simulation
	if(bSynchronous)
	{
		PerformSimulation(WorkingModel, SimulationDuration, false, BuildMesh, true);
	}
	else
	{
		UE_LOG(LogTetherCable, VeryVerbose, TEXT("%s: ResimulateInvalidatedSegments -> PerformAsyncSimulation, bBuildMesh: %i"), *GetHumanReadableName(), (int32)BuildMesh);
		PerformAsyncSimulation(WorkingModel, SimulationDuration, true, BuildMesh);
	}
}

void ATetherCableActor::UpdateInvalidSimulationSegmentsSplineInfo(FTetherSimulationModel& Model)
{
	UE_LOG(LogTetherCable, VeryVerbose, TEXT("%s: Rebuilding segments"), *GetHumanReadableName());
	
	for(int32 i=0;i< Model.Segments.Num();i++)
	{
		FTetherSimulationSegment& Segment = Model.Segments[i];

		UE_LOG(LogTetherCable, VeryVerbose, TEXT("%s: Segment %i"), *GetHumanReadableName(), i);
		UE_LOG(LogTetherCable, VeryVerbose, TEXT("%s:     Invalidated: %i"), *GetHumanReadableName(), (int32)Model.Segments[i].IsInvalidated());
		
		if(!Segment.IsInvalidated())
		{
			// Skip segments that haven't been invalidated
			continue;
		}

		Segment.SplineSegmentInfo = FCableSplineUtils::SegmentInfoFromSplineComponent(GuideSpline, i, GetCableRootTransform());
		 
		UE_LOG(LogTetherCable, VeryVerbose, TEXT("%s:     Start: %s"), *GetHumanReadableName(), *Segment.SplineSegmentInfo.StartLocation.ToCompactString());
		UE_LOG(LogTetherCable, VeryVerbose, TEXT("%s:     End: %s"), *GetHumanReadableName(), *Segment.SplineSegmentInfo.EndLocation.ToCompactString());

		const float StartDistance = GuideSpline->GetDistanceAlongSplineAtSplinePoint(i);
		const float EndDistance = GuideSpline->GetDistanceAlongSplineAtSplinePoint(i + 1);
		const float Distance = EndDistance - StartDistance; //FVector::Distance(StartLocation, EndLocation);

		Segment.Length = FMath::Max(Distance + GetPointSegmentDefinitions()[i]->Slack, 0.01f); // Use a non-zero minimum, we never want zero length
		UE_LOG(LogTetherCable, VeryVerbose, TEXT("%s:     Length: %f"), *GetHumanReadableName(), Segment.Length);

		ensure(Segment.IsValid());
	}

	ValidateTangents(Model, GetPointSegmentDefinitions());
	
}

void ATetherCableActor::ResetInvalidatedSegmentsSimulationState(FTetherSimulationModel& Model)
{
	UE_LOG(LogTetherCable, Verbose, TEXT("%s: ResetInvalidatedSegmentsSimulationState"), *GetHumanReadableName());

	TArray<FTetherProxySimulationSegmentSeries> SimulationSeries = MakeSimulationParams(Model).MakeSegmentSeriesToSimulate(Model, true);

	int32 NumSegmentsRebuilt = 0;
	for(int32 i=0; i< SimulationSeries.Num();i++)
	{
		FTetherProxySimulationSegmentSeries& Series = SimulationSeries[i];
		
		// Work out whether to rebuild the whole series (if any contained segments were invalidated)
		bool bRebuildSeries = false;
		for(FTetherSimulationSegment* Segment : Series.Segments)
		{
			if(Segment->IsInvalidated())
			{
				bRebuildSeries = true;
				break;
			}
		}
		
		for (FTetherSimulationSegment* Segment : Series.Segments)
		{
			UE_LOG(LogTetherCable, VeryVerbose, TEXT("%s: Segment %i"), *GetHumanReadableName(), Segment->SegmentUniqueId);
			UE_LOG(LogTetherCable, VeryVerbose, TEXT("%s:     Start: %s"), *GetHumanReadableName(), *Segment->SplineSegmentInfo.StartLocation.ToCompactString());
			UE_LOG(LogTetherCable, VeryVerbose, TEXT("%s:     End: %s"), *GetHumanReadableName(), *Segment->SplineSegmentInfo.EndLocation.ToCompactString());
			UE_LOG(LogTetherCable, VeryVerbose, TEXT("%s:     Length: %f"), *GetHumanReadableName(), Segment->Length);
			UE_LOG(LogTetherCable, VeryVerbose, TEXT("%s:     Invalidated: %i"), *GetHumanReadableName(), (int32)Segment->IsInvalidated());
			UE_LOG(LogTetherCable, VeryVerbose, TEXT("%s:     Rebuild: %i"), *GetHumanReadableName(), (int32)bRebuildSeries);
			
			if(bRebuildSeries)
			{
				BuildParticlesForSegment(Model, Segment->SegmentUniqueId);
				NumSegmentsRebuilt++;
			}
		}
		
	}

	UE_LOG(LogTetherCable, Verbose, TEXT("%s: Rebuilt %i segments"), *GetHumanReadableName(), NumSegmentsRebuilt);

	uint32 NextIndex = 0;
	for(FTetherSimulationSegment& Segment : Model.Segments)
	{
		for(FTetherSimulationParticle& Particle : Segment.Particles)
		{
			Particle.ParticleUniqueId = NextIndex;
			NextIndex++;
		}
	}
	
}

void ATetherCableActor::BuildParticlesForSegment(FTetherSimulationModel& Model, int32 SegmentIndex)
{
	TArray<FTetherSimulationSegment>& Segments = Model.Segments;

	ensure(Segments.IsValidIndex(SegmentIndex));

	FTetherSimulationSegment& Segment = Segments[SegmentIndex];

	const bool bStartFixed = GetPointSegmentDefinitions()[SegmentIndex]->SimulationOptions.bFixedAnchorPoint;
	const bool bEndFixed = GetPointSegmentDefinitions()[SegmentIndex + 1]->SimulationOptions.bFixedAnchorPoint;
	Segment.BuildParticles(CableProperties.GetDesiredParticleDistance(), bStartFixed, bEndFixed);
}

bool ATetherCableActor::PrepareForSimulation(FTetherSimulationModel& InitialModel, FTetherSimulationParams& Params)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("ATetherCableActor::PrepareForSimulation"))

	if(!ensure(!IsRunningAsyncSimulation()))
	{
		return false;
	}

	if(!ensure(InitialModel.IsValid()))
	{
		return false;
	}

	if(!ensure(ValidateTangents(InitialModel, GetPointSegmentDefinitions())))
	{
		return false;
	}

	if(!ensure(Params.World->GetPhysicsScene()))
	{
		return false;
	}

	if(!ensure(InitialModel.Segments.Num() == Params.SegmentParams.Num() - 1))
	{
		return false;
	}
	
	// Set base world transform on the simulation model
	InitialModel.SimulationBaseWorldTransform = GetActorTransform();

	return true;
}


FTetherSimulationResultInfo ATetherCableActor::PerformSimulation(FTetherSimulationModel& InitialModel, float DeltaTime, bool bIgnoreSegmentsSimulatingAsync, EMeshBuildInstruction BuildMesh, bool bVerboseLogging)
{
	checkNoRecursion();
	
	UE_LOG(LogTetherCable, Verbose, TEXT("%s: PerformSimulation - bIgnoreSegmentsSimulatingAsync %i - bBuildMesh %i"), *GetHumanReadableName(), (int32)bIgnoreSegmentsSimulatingAsync, (int32)BuildMesh);
	
	// Note: Even if this version is synchronous, it's not always the realtime preview
	// It can also be synchronous rebuild through the details panel
	
	if(bLockCurrentState)
	{
		return DeltaTime;
	}

	if (!ensure(InitialModel.HasAnyParticles()))
	{
		return DeltaTime;
	}

	FTetherSimulationParams Params = MakeSimulationParams(InitialModel);
	const bool bCanSimulate = PrepareForSimulation(InitialModel, Params);
	if(!bCanSimulate)
	{
		return DeltaTime;
	}

	FTetherSimulationInstanceResources Resources;
	Resources.InitializeResources(InitialModel, Params);

	if(bIgnoreSegmentsSimulatingAsync)
	{
		// Simulate segments that aren't already async simulating
		for (int i = 0; i < ActiveSimulationModel.Segments.Num(); i++)
		{
			if (CurrentSegmentsRunningAsyncSimulation.Contains(i))
			{
				Params.SegmentParams[i].bShouldSimulateSegment = false;
			}
		}
	}
	
	const FTetherSimulationResultInfo ResultInfo = FTetherSimulation::PerformSimulation(InitialModel, DeltaTime, Params);

	Resources.ReleaseResources();

	HandleSimulationComplete(ResultInfo, InitialModel, Params, BuildMesh, bVerboseLogging, true);
	return ResultInfo;
}

bool ATetherCableActor::PerformAsyncSimulation(FTetherSimulationModel& InitialModel, float DeltaTime, bool bOnlySimulateInvalidatedSegments, EMeshBuildInstruction BuildMesh, FOnTetherAsyncSimulationCompleteDelegate CompleteCallback)
{
	checkNoRecursion();
	
	if(bLockCurrentState)
	{
		return false;
	}
	
	if(IsRunningAsyncSimulation())
	{
		return false;
	}

	if(!ensure(InitialModel.HasAnyParticles()))
	{
		return false;
	}

	if(!GetWorld()->GetPhysicsScene())
	{
		return false;
	}

	FTetherSimulationParams Params = MakeSimulationParams(InitialModel);
	
	TArray<int32> SegmentsToSimulate;
	if(bOnlySimulateInvalidatedSegments)
	{
		SegmentsToSimulate = InitialModel.GetInvalidatedSegments();
	}
	else
	{
		SegmentsToSimulate = InitialModel.GetSegmentUniqueIds();
	}

	if(!ensure(SegmentsToSimulate.Num() > 0))
	{
		return false;
	}

	Params.SetSegmentsToSimulate(SegmentsToSimulate);

	const TArray<FTetherProxySimulationSegmentSeries> SimulationSeries = Params.MakeSegmentSeriesToSimulate(InitialModel, true);
	if(SimulationSeries.Num() < 1)
	{
		return false;
	}

	const bool bCanSimulate = PrepareForSimulation(InitialModel, Params);
	if(!bCanSimulate)
	{
		return false;
	}

	CurrentSegmentsRunningAsyncSimulation = SegmentsToSimulate;

	float StartTime = GetWorld()->GetRealTimeSeconds();
	UE_LOG(LogTetherCable, Verbose, TEXT("%s: Starting async simulation - bBuildMesh: %i, StartTime: %f"), *GetHumanReadableName(), (int32)BuildMesh, StartTime);
	
	// Create lambda for task completion
	FOnTetherAsyncSimulationCompleteDelegate TaskCompleteCallback = FOnTetherAsyncSimulationCompleteDelegate::CreateWeakLambda(this, [this, Params, StartTime, BuildMesh, CompleteCallback]
	(const FTetherSimulationModel& SimulatedModel, const FTetherSimulationResultInfo& ResultInfo)
	{
		if(CurrentSimulationTask)
		{
			check(CurrentSimulationTask->IsDone());
			CurrentSimulationTask = nullptr;
		}
		
		// Async task finished and returned back to main thread
		const float AsyncTime = GetWorld()->GetRealTimeSeconds() - StartTime;

		UE_LOG(LogTetherCable, Verbose, TEXT("%s: Async simulation complete, CurrentTime: %f, took %f seconds"), *GetHumanReadableName(), FApp::GetCurrentTime(), AsyncTime);

		CurrentSegmentsRunningAsyncSimulation = {};

		// Check if we need to simulate again, if the state was invalidated while it was simulating
		bool bShouldSimulateAgain = false;

		if(!bRealtimeSimulating)
		{
			// First rebuild modified segments without simulating (no simulating or building here)
			UpdateAndRebuildModifiedSegments(false, DoNotBuild, false);
			// Check if any segment is invalidated, if so we need to simulate again
			if(IsAnySegmentInvalidated())
			{
				bShouldSimulateAgain = true;
			}
		}

		// Handle simulation complete
		// Only build mesh for the completed simulation if we aren't going to simulate again
		const EMeshBuildInstruction HandleSimulationBuildMesh = bShouldSimulateAgain ? DoNotBuild : BuildMesh;
		HandleSimulationComplete(ResultInfo, SimulatedModel, Params, HandleSimulationBuildMesh, true);

		if(bShouldSimulateAgain)
		{
			ResimulateInvalidatedSegments(false, BuildMesh);
		}	

		CompleteCallback.ExecuteIfBound(SimulatedModel, ResultInfo);
	});

	// Kick off async task on worker thread
	check(!CurrentSimulationTask);
	CurrentSimulationTask = new FAsyncTaskExecuterWithAbort<FTetherAsyncSimulationTask>(TaskCompleteCallback, InitialModel, DeltaTime, Params);
	CurrentSimulationTask->StartBackgroundTask();

	return true;
}

void ATetherCableActor::RealtimeSimulationComplete(const FTetherSimulationModel& SimulatedModel, const FTetherSimulationResultInfo& ResultInfo)
{
	RealtimeSimulationTimeRemainder -= ResultInfo.SimulatedTime;
	UE_LOG(LogTetherCable, VeryVerbose, TEXT("%s: RealtimeSimulationComplete, SimulatedTime: %f, RealtimeSimulationTimeremainder: %f"), *GetHumanReadableName(), ResultInfo.SimulatedTime, RealtimeSimulationTimeRemainder);
}

FVector ATetherCableActor::GetCableForce() const
{
	// Just use normal world gravity
	return FVector(0, 0, GetWorld()->GetGravityZ());
}

void ATetherCableActor::DrawDebugSimulationParticleCollision(float Lifetime)
{
	for(int i=0; i< ActiveSimulationModel.Segments.Num(); i++)
	{
		const FTetherSimulationSegment& Segment = ActiveSimulationModel.Segments[i];
		TArray<FVector> Points = Segment.GetParticleLocations();
		FTetherDebugDrawHelpers::DrawSpheres(GetWorld(), Points, CableProperties.GetCollisionWidth() * 0.5f, Segment.IsInvalidated() || CurrentSegmentsRunningAsyncSimulation.Contains(i) ? FColor::Red : FColor::White, Lifetime);
	}
}

FVector ATetherCableActor::GetGuideSplinePointWorldLocation(int32 PointIndex) const
{
	// Note: We retrieve the point in local space here and convert it to world using the actor position, because the relative transform of the spline can be wrong while loading
	const FVector LocalPoint = GuideSpline->GetLocationAtSplinePoint(PointIndex, ESplineCoordinateSpace::Type::Local);
	return CableRootTransformPosition(LocalPoint);
}

FTetherSimulationParams ATetherCableActor::MakeSimulationParams(FTetherSimulationModel& ModelForSimulation) const
{
	ensure(IsValid(GetWorld()));

	ensure(!GetWorld()->IsGameWorld());
	
	FTetherSimulationParams Params(GetWorld());

	Params.OwningActor = this;
	Params.Component = DynamicPreviewMesh;

	Params.SimulationName = GetHumanReadableName();

	Params.CollisionWidth = CableProperties.GetCollisionWidth();
	Params.DesiredParticleDistance = CableProperties.GetDesiredParticleDistance();
	Params.CableForce = GetCableForce();
	Params.SimulationOptions = CableProperties.SimulationOptions;

	// Don't collide with our existing components
	Params.CollisionQueryParams.AddIgnoredComponent(StaticMeshComponent);

	for (TActorIterator<ATetherCableActor> ActorItr(GetWorld()); ActorItr; ++ActorItr)
	{
		ATetherCableActor* Cable = *ActorItr;
		if(Cable != this && ShouldSimulateBefore(Cable))
		{
			Params.CollisionQueryParams.AddIgnoredActor(Cable);
		}
	}
	
	// Trace complex so that we hit existing cables
	Params.CollisionQueryParams.bTraceComplex = true;

	const int32 NumPoints = GetNumGuideSplinePoints();
	
	// Add segments
	for(int32 SegmentIndex = 0; SegmentIndex < NumPoints; SegmentIndex++)
	{
		Params.SegmentParams.AddDefaulted();
		Params.SegmentParams[SegmentIndex].SimulationOptions = GetPointSegmentDefinitions()[SegmentIndex]->SimulationOptions;
	}

	Params.SegmentParams.Last().bShouldSimulateSegment = false;
	
	return Params;
}

void ATetherCableActor::HandleSimulationComplete(const FTetherSimulationResultInfo& ResultInfo, const FTetherSimulationModel& SimulatedModel, const FTetherSimulationParams& Params, EMeshBuildInstruction BuildMesh, bool bVerboseLogging, bool bSynchronous)
{
	if(bVerboseLogging)
	{
		UE_LOG(LogTetherCable, Verbose, TEXT("%s: HandleSimulationComplete - bBuildMesh %i"), *GetHumanReadableName(), (int32)BuildMesh);
	}

	if(bVerboseLogging)
	{
		UE_LOG(LogTetherCable, Verbose, TEXT("%s: Release resources because simulation has returned"), *GetHumanReadableName());
	}

	// Pass simulated particles back into active model
	// Preserve invalidation of the active model, and segments that weren't simulated
	// Only copy particles for the segments that were actually simulated, rather than replacing the entire simulation model
	for (int i = 0; i < SimulatedModel.Segments.Num(); i++)
	{
		if (!ResultInfo.SimulatedSegments.Contains(i))
		{
			// Skip if not simulated
			continue;
		}

		if (!ActiveSimulationModel.Segments.IsValidIndex(i))
		{
			// Segment was removed, so ignore it
			continue;
		}

		// Update particles of active simulation
		ActiveSimulationModel.Segments[i].Particles = SimulatedModel.Segments[i].Particles;
		ActiveSimulationModel.Segments[i].SimulationTime = SimulatedModel.Segments[i].SimulationTime;
	}

	ActiveSimulationModel.SimulationBaseWorldTransform = SimulatedModel.SimulationBaseWorldTransform;
	
	// Log thread-unsafe information that was gathered during the simulation
	for (TWeakObjectPtr<UPrimitiveComponent> Component : ResultInfo.HitComponents)
	{
		if(bVerboseLogging)
		{
			const bool bValidComponent = IsValid(Component.Get());
			UE_LOG(LogTetherSimulation, Verbose, TEXT("%s: Hit component: %s"), *GetHumanReadableName(), bValidComponent ? *Component.Get()->GetReadableName() : TEXT("Invalid"));
		}
	}

	// Update mesh positions
	DynamicPreviewMesh->SetAbsolute(true, true, true);
	StaticMeshComponent->SetAbsolute(true, true, true);
	DynamicPreviewMesh->SetWorldTransform(ActiveSimulationModel.SimulationBaseWorldTransform);
	StaticMeshComponent->SetWorldTransform(ActiveSimulationModel.SimulationBaseWorldTransform);

	// Make curve description and check if it's been modified since the last time the static mesh was built
	const FCableMeshGenerationCurveDescription CurveDescriptionLocalSpaceSimplified = MakeMeshGenerationCurveDescriptionFromCurrentSimulationState(true);
	const bool bCurveDescriptionModified = CurveDescriptionLocalSpaceSimplified != BuiltCurveDescriptionLocalSpaceSimplified;

	EMeshBuildInstruction BuildMeshNow = DoNotBuild;
	bool bBuildStarted = false;
	
	if(CanAutoBuild())
	{
		if (BuildMesh > BuildMeshNow)
		{
			BuildMeshNow = BuildMesh;
			UE_LOG(LogTetherCable, Verbose, TEXT("%s: bBuildMesh true, so will try to build now"), *GetHumanReadableName());
		}

		if (BuildAfterCurrentSimulation > BuildMeshNow)
		{
			BuildMeshNow = BuildAfterCurrentSimulation;
			UE_LOG(LogTetherCable, Verbose, TEXT("%s: Static mesh was queued, so try to build now that simulation has returned"), *GetHumanReadableName());
		}

		if (BuildMeshNow)
		{
			const bool bForceEvenIfNotModified = BuildMeshNow == AlwaysBuild;
			BuildAfterCurrentSimulation = DoNotBuild;
			UE_LOG(LogTetherCable, VeryVerbose, TEXT("%s: HandleSimulationComplete -> QueueBuildAndShowStaticMesh"), *GetHumanReadableName());
			bBuildStarted = QueueBuildAndShowStaticMesh(bForceEvenIfNotModified, bSynchronous);
		}
	}
	else
	{
		UE_LOG(LogTetherCable, Verbose, TEXT("%s: Auto-build not allowed for this MeshGenerator"), *GetHumanReadableName());
	}
	
	if(!bBuildStarted && bCurveDescriptionModified)
	{
		// If the new mesh would not match the current static mesh, switch to dynamic preview
		UE_LOG(LogTetherCable, VeryVerbose, TEXT("%s: Curve description was MODIFIED by simulation, rebuild dynamic preview"), *GetHumanReadableName());
		DynamicPreviewMesh->SetMeshGenerationParams(MakeMeshGenerationCurveDescriptionFromCurrentSimulationState(false), GetPreviewMeshGenerationOptions());
		UpdateMeshVisibilities(false, false);
	}
	else
	{
		// Static mesh is already up to date
		// This could happen when previewing a cable that already had the exact mesh built with the exact same setup
		// In that case show the existing static mesh, which is still up to date, in the preview location
		UE_LOG(LogTetherCable, VeryVerbose, TEXT("%s: Curve description was not NOT MODIFIED by simulation"), *GetHumanReadableName());
		// Still ensure we are on static visibility
		UpdateMeshVisibilities(true, true);
	}

	OnSimulationUpdated.Broadcast();

	GEditor->RedrawAllViewports();

	// Cable length into custom primitive float 0
	StaticMeshComponent->SetCustomPrimitiveDataFloat(0, ActiveSimulationModel.GetLength());
}

void ATetherCableActor::BlockUntilSimulationComplete()
{
	// This doesn't really work currently, since it also blocks the GT return handler from finishing the task
	check(0);
	bool bBlocked = false;
	if(IsRunningAsyncSimulation())
	{
		UE_LOG(LogTetherCable, Log, TEXT("%s: Blocking main thread while simulation finishes"), *GetHumanReadableName());
		const float StartTime = GetWorld()->GetRealTimeSeconds();
		while (IsRunningAsyncSimulation())
		{
			FPlatformProcess::Sleep(.01f);
			bBlocked = true;
		}
		UE_LOG(LogTetherCable, Log, TEXT("%s: Blocked for %f seconds"), *GetHumanReadableName(), GetWorld()->GetRealTimeSeconds() - StartTime);
	}
}

#endif
