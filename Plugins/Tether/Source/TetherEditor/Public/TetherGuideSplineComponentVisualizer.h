// Copyright Sam Bonifacio 2021. All Rights Reserved.

#pragma once
#include "SplineComponentVisualizer.h"
#include "Misc/EngineVersionComparison.h"

class TETHEREDITOR_API FTetherGuideSplineComponentVisualizer : public FSplineComponentVisualizer
{
public:

	virtual void OnRegister() override;

	virtual void GenerateContextMenuSections(FMenuBuilder& InMenuBuilder) const override;
#if UE_VERSION_OLDER_THAN(5,0,0)
	virtual void SplitSegment(const FVector& InWorldPos, int32 InSegmentIndex) override;
#else
	virtual void SplitSegment(const FVector& InWorldPos, int32 InSegmentIndex, bool bCopyFromSegmentBeginIndex) override;
#endif
	
    void SelectAllPoints();

	void SelectPoint(int32 PointIndex);

	void DeleteSelectedPoints();

	void SetEditedComponent(USplineComponent* Component);

};
