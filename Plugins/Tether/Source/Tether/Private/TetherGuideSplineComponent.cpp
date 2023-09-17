// Copyright Sam Bonifacio 2021. All Rights Reserved.

#include "TetherGuideSplineComponent.h"
#include "TimerManager.h"

UTetherGuideSplineMetadata::UTetherGuideSplineMetadata()
{

}

void UTetherGuideSplineMetadata::InsertPoint(int32 Index, float t, bool bClosedLoop)
{
	// Seems like t should be the ratio between the two existing points where the new one should be inserted
	// But seems to be called with Index as the value (in USplineComponent::AddSplinePointAtIndex) so filter that out
	const float Ratio = t == Index ? 1.f : t;
	
	Modify();
	UTetherPointSegmentDefinition* NewSegmentDefinition;

	const bool bRightSide = Ratio > 0.5f;
	if(bRightSide && PointSegmentDefinitions.IsValidIndex(Index))
	{
		// Duplicate next point
		NewSegmentDefinition = DuplicatePointSegmentDefinition(PointSegmentDefinitions[Index]);
	}
	else if(!bRightSide && PointSegmentDefinitions.IsValidIndex(Index - 1))
	{
		// Duplicate previous point
		NewSegmentDefinition = DuplicatePointSegmentDefinition(PointSegmentDefinitions[Index - 1]);
	}
	else
	{
		// Create new
		NewSegmentDefinition = CreatePointSegmentDefinition();
	}
	PointSegmentDefinitions.Insert(NewSegmentDefinition, Index);
	BroadcastModified();
}

void UTetherGuideSplineMetadata::UpdatePoint(int32 Index, float t, bool bClosedLoop)
{
}

void UTetherGuideSplineMetadata::AddPoint(float InputKey)
{
	Modify();
	const int32 Index = FMath::CeilToInt(InputKey);
	UTetherPointSegmentDefinition* NewSegmentDefinition = CreatePointSegmentDefinition();
	PointSegmentDefinitions.Insert(NewSegmentDefinition, Index);
	BroadcastModified();
}

void UTetherGuideSplineMetadata::RemovePoint(int32 Index)
{
	if(PointSegmentDefinitions.IsValidIndex(Index))
	{
		Modify();
		UTetherPointSegmentDefinition* SegmentToRemove = PointSegmentDefinitions[Index];
		PointSegmentDefinitions.RemoveAt(Index);
		BroadcastModified(Index);
	}
}

void UTetherGuideSplineMetadata::DuplicatePoint(int32 Index)
{
	Modify();
	UTetherPointSegmentDefinition* NewSegmentDefinition = DuplicatePointSegmentDefinition(PointSegmentDefinitions[Index]);
	PointSegmentDefinitions.Insert(NewSegmentDefinition, Index + 1);
	BroadcastModified();
}

void UTetherGuideSplineMetadata::CopyPoint(const USplineMetadata* FromSplineMetadata, int32 FromIndex, int32 ToIndex)
{
	const UTetherGuideSplineMetadata* FromTetherGuideSplineMetadata = Cast<UTetherGuideSplineMetadata>(FromSplineMetadata);
	if(IsValid(FromTetherGuideSplineMetadata))
	{
		Modify();
		UTetherPointSegmentDefinition* NewSegmentDefinition = DuplicatePointSegmentDefinition(FromTetherGuideSplineMetadata->PointSegmentDefinitions[FromIndex]);
		RemovePoint(ToIndex);
		PointSegmentDefinitions.Insert(NewSegmentDefinition, ToIndex);
		BroadcastModified();
	}
}

void UTetherGuideSplineMetadata::Reset(int32 NumPoints)
{
	Modify();
	for(int32 i = PointSegmentDefinitions.Num() - 1; i >= 0 ; i--)
	{
		RemovePoint(i);
	}
	
	for(int32 i = 0; i< NumPoints; i++)
	{
		InsertPoint(i, i, false);
	}
	BroadcastModified();
}

void UTetherGuideSplineMetadata::Fixup(int32 NumPoints, USplineComponent* SplineComp)
{
	ensure(0);
}

FName UTetherGuideSplineMetadata::MakeUniqueName()
{
	return MakeUniqueObjectName(this, UTetherPointSegmentDefinition::StaticClass(), "PointSegmentDefinition");
}

UTetherPointSegmentDefinition* UTetherGuideSplineMetadata::CreatePointSegmentDefinition()
{
	return NewObject<UTetherPointSegmentDefinition>(this, MakeUniqueName());
}

UTetherPointSegmentDefinition* UTetherGuideSplineMetadata::DuplicatePointSegmentDefinition(const UTetherPointSegmentDefinition* Other)
{
	return DuplicateObject<UTetherPointSegmentDefinition>(Other, this, MakeUniqueName());
}

void UTetherGuideSplineMetadata::BroadcastModified(int32 RemovedPointIndex)
{
	UTetherGuideSplineComponent* Comp = Cast<UTetherGuideSplineComponent>(GetOuter());
	if(IsValid(Comp) && IsValid(GetWorld()))
	{
		if(IsValid(Comp))
		{
			if(RemovedPointIndex >= 0)
			{
				Comp->OnPointRemoved.Broadcast(RemovedPointIndex);
			}
			Comp->OnModified.Broadcast();
		}
	}
}

UTetherGuideSplineComponent::UTetherGuideSplineComponent()
{
	Metadata = CreateDefaultSubobject<UTetherGuideSplineMetadata>(TEXT("Metadata"));
	
	if(HasAnyFlags(RF_ArchetypeObject))
	{
		// Don't create points under CDO, as the new objects will cause "graph linked to external private object" when saving
		return;
	}

	Metadata->AddPoint(0);
	Metadata->AddPoint(1);

	ensure(Metadata->GetPointSegmentDefinitions().Num() == GetNumberOfSplinePoints());
}
