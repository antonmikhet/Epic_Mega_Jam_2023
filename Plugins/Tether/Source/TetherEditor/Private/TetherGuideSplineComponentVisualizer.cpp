// Copyright Sam Bonifacio 2021. All Rights Reserved.

#include "TetherGuideSplineComponentVisualizer.h"
#include "EditorModeManager.h"
#include "TetherCableActor.h"
#include "TetherEditorStatics.h"
#include "TetherEdMode.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Misc/EngineVersionComparison.h"

void FTetherGuideSplineComponentVisualizer::OnRegister()
{
	FSplineComponentVisualizer::OnRegister();
}

void FTetherGuideSplineComponentVisualizer::GenerateContextMenuSections(FMenuBuilder& InMenuBuilder) const
{
	InMenuBuilder.BeginSection("Tether", INVTEXT("Tether"));
	{
		InMenuBuilder.AddMenuEntry(
			INVTEXT("Edit Point"),
			INVTEXT("Edit this point in the Tether Cable mode"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([this]()
				{
					ATetherCableActor* Cable = Cast<ATetherCableActor>(GetEditedSplineComponent()->GetOwner());
					if(!ensure(IsValid(Cable)))
					{
						return;
					}
					FTetherEdMode* EdMode = FTetherEditorStatics::ActivateTetherMode();
					if(!ensure(EdMode))
					{
						return;
					}
#if UE_VERSION_OLDER_THAN(4,26,0)
					const int32 Key = LastKeyIndexSelected;
#else
					const int32 Key = SelectionState->GetLastKeyIndexSelected();
#endif
					EdMode->StartEditing(Cable, Key, true);
				}),
				FCanExecuteAction::CreateLambda([] { return true; })
			)
		);

		InMenuBuilder.AddMenuEntry(
			INVTEXT("Continue Cable"),
			INVTEXT("Continue the cable after this point in the Tether Cable mode"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([this]()
		{
			ATetherCableActor* Cable = Cast<ATetherCableActor>(GetEditedSplineComponent()->GetOwner());
			if (!ensure(IsValid(Cable)))
			{
				return;
			}
			FTetherEdMode* EdMode = FTetherEditorStatics::ActivateTetherMode();
			if (!ensure(EdMode))
			{
				return;
			}
#if UE_VERSION_OLDER_THAN(4,26,0)
			const int32 Key = LastKeyIndexSelected;
#else
			const int32 Key = SelectionState->GetLastKeyIndexSelected();
#endif
			EdMode->StartEditing(Cable, Key, true);
			EdMode->StartNewSegment(Cable->GetPointLocation(Key), false);
		}),
				FCanExecuteAction::CreateLambda([] { return true; })
			)
		);
	}
	InMenuBuilder.EndSection();
	
	FSplineComponentVisualizer::GenerateContextMenuSections(InMenuBuilder);
}

#if UE_VERSION_OLDER_THAN(5,0,0)
void FTetherGuideSplineComponentVisualizer::SplitSegment(const FVector& InWorldPos, int32 InSegmentIndex)
#else
void FTetherGuideSplineComponentVisualizer::SplitSegment(const FVector& InWorldPos, int32 InSegmentIndex, bool bCopyFromSegmentBeginIndex)
#endif
{
	// Hack: Base implementation seems to intentionally remove tangents for some reason by changing mode from CurveUser to CurveAuto
	// but we don't want that, so undo it if it happened
	
	USplineComponent* SplineComp = GetEditedSplineComponent();
	check(SplineComp != nullptr);
	SplineComp->Modify();
	FInterpCurveVector& SplinePosition = SplineComp->GetSplinePointsPosition();

	// Cache old points
	
	const int32 PrevPointIndex = InSegmentIndex;
	const FInterpCurvePointVector PrevPoint = SplinePosition.Points[PrevPointIndex];
	
	const int32 NextPointIndex = InSegmentIndex+1;
	const FInterpCurvePointVector NextPoint = SplinePosition.Points[NextPointIndex];

#if UE_VERSION_OLDER_THAN(5,0,0)
	FSplineComponentVisualizer::SplitSegment(InWorldPos, InSegmentIndex);
#else
	FSplineComponentVisualizer::SplitSegment(InWorldPos, InSegmentIndex, bCopyFromSegmentBeginIndex);
#endif
	
	const int32 NextPointNewIndex = InSegmentIndex+2;

	// Apply old data to new points to preserve tangents
	
	if(PrevPoint.InterpMode == CIM_CurveUser)
	{
		FInterpCurvePointVector& UpdatedPrevPoint = SplinePosition.Points[PrevPointIndex];
		UpdatedPrevPoint.InterpMode = CIM_CurveUser;
		UpdatedPrevPoint.ArriveTangent = PrevPoint.ArriveTangent;
		UpdatedPrevPoint.LeaveTangent = PrevPoint.LeaveTangent;
	}
	if(NextPoint.InterpMode == CIM_CurveUser)
	{
		FInterpCurvePointVector& UpdatedNextPoint = SplinePosition.Points[NextPointNewIndex];
		UpdatedNextPoint.InterpMode = CIM_CurveUser;
		UpdatedNextPoint.ArriveTangent = NextPoint.ArriveTangent;
		UpdatedNextPoint.LeaveTangent = NextPoint.LeaveTangent;
	}
}

void FTetherGuideSplineComponentVisualizer::SelectAllPoints()
{
    OnSelectAllSplinePoints();
}

void FTetherGuideSplineComponentVisualizer::SelectPoint(int32 PointIndex)
{
	ChangeSelectionState(PointIndex, false);
}

void FTetherGuideSplineComponentVisualizer::DeleteSelectedPoints()
{
	OnDeleteKey();
}

void FTetherGuideSplineComponentVisualizer::SetEditedComponent(USplineComponent* Component)
{
#if UE_VERSION_OLDER_THAN(4,26,0)
	SplinePropertyPath = FComponentPropertyPath(Component);
#else
	SelectionState->SetSplinePropertyPath(FComponentPropertyPath(Component));
#endif
}
