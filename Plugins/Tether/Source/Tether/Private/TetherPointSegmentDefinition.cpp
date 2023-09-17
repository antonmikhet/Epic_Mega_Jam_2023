// Copyright Sam Bonifacio 2021. All Rights Reserved.

#include "TetherPointSegmentDefinition.h"
#include "TetherCableActor.h"

#if WITH_EDITOR

bool UTetherPointSegmentDefinition::CanEditChange(const FProperty* InProperty) const
{
	if(!Super::CanEditChange(InProperty))
	{
		return false;
	}

	return !GetWorld()->IsGameWorld();
}

void UTetherPointSegmentDefinition::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// Forward to cable to trigger resimulation
	// Hardcoded for now to expect outer chain of Metadata > Spline > Cable Actor
	ATetherCableActor* OuterCable = Cast<ATetherCableActor>(GetOuter()->GetOuter()->GetOuter());
	if (IsValid(OuterCable))
	{
		OuterCable->HandlePropertyChanged(PropertyChangedEvent);
	}
}
#endif
