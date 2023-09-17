// Copyright Sam Bonifacio 2021. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TetherPointSegmentDefinition.h"
#include "Components/SplineComponent.h"
#include "TetherGuideSplineComponent.generated.h"

/**
 * SplineMetadata subclass used to implement spline modification calls to store information about each point/segment
 */
UCLASS()
class TETHER_API UTetherGuideSplineMetadata : public USplineMetadata
{
	GENERATED_BODY()

public:

	UTetherGuideSplineMetadata();

	TArray<UTetherPointSegmentDefinition*> GetPointSegmentDefinitions() const { return PointSegmentDefinitions; }

	virtual void InsertPoint(int32 Index, float t, bool bClosedLoop) override;
	virtual void UpdatePoint(int32 Index, float t, bool bClosedLoop) override;
	virtual void AddPoint(float InputKey) override;
	virtual void RemovePoint(int32 Index) override;
	virtual void DuplicatePoint(int32 Index) override;
	virtual void CopyPoint(const USplineMetadata* FromSplineMetadata, int32 FromIndex, int32 ToIndex) override;
	virtual void Reset(int32 NumPoints) override;
	virtual void Fixup(int32 NumPoints, USplineComponent* SplineComp) override;
	
private:

	// User-defined settings per point/segment
	// May exist for segments that no longer exist in the simulation
	UPROPERTY()
	TArray<UTetherPointSegmentDefinition*> PointSegmentDefinitions;

	FName MakeUniqueName();
	UTetherPointSegmentDefinition* CreatePointSegmentDefinition();
	UTetherPointSegmentDefinition* DuplicatePointSegmentDefinition(const UTetherPointSegmentDefinition* Other);

	void BroadcastModified(int32 RemovedPointIndex = -1);
};

DECLARE_MULTICAST_DELEGATE(FTetherGuideSplineModified);
DECLARE_MULTICAST_DELEGATE_OneParam(FTetherGuideSplinePointRemoved, int32);

/**
 * Special subclass of SplineComponent used by Tether cables to store information about each point/segment
 */
UCLASS()
class TETHER_API UTetherGuideSplineComponent : public USplineComponent
{
	GENERATED_BODY()

public:

	UTetherGuideSplineComponent();

	// Note: This delegate is fired while the Spline Component is still operating on itself. Be very careful about modifying or querying the Spline Component from this
	FTetherGuideSplineModified OnModified;
	// Note: This delegate is fired while the Spline Component is still operating on itself. Be very careful about modifying or querying the Spline Component from this
	FTetherGuideSplinePointRemoved OnPointRemoved;

	TArray<UTetherPointSegmentDefinition*> GetPointSegmentDefinitions() const { return Metadata->GetPointSegmentDefinitions(); }

protected:

	virtual USplineMetadata* GetSplinePointsMetadata() override { return Metadata; }
	virtual const USplineMetadata* GetSplinePointsMetadata() const override { return Metadata; }

private:

	UPROPERTY()
	UTetherGuideSplineMetadata* Metadata;
	
};
