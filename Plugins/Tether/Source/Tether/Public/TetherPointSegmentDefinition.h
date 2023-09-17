// Copyright Sam Bonifacio 2021. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TetherCompatibility.h"
#include "Simulation/TetherSegmentSimulationOptions.h"
#include "TetherPointSegmentDefinition.generated.h"


/**
 * Represents the definition of a single segment or point of a Tether cable
 * This is a UObject and not a struct so that the details view can edit the combined properties of multiple selected points (see TetherCablePointDetails)
 */
UCLASS()
class TETHER_API UTetherPointSegmentDefinition : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Slack is added to (or removed from) the segment length to alter the tightness of the cable
	 * Cables with more slack may naturally coil up on the ground as part of the simulation
	 */
	UPROPERTY(Category = "TetherProperties", EditAnywhere, BlueprintReadWrite, meta = (UIMax = 10000))
	float Slack = 0.f;

	// Options that control how this specific segment or point is simulated
	UPROPERTY(Category = "TetherProperties", EditAnywhere, BlueprintReadWrite)
	FTetherSegmentSimulationOptions SimulationOptions;

protected:

#if WITH_EDITOR
	virtual bool CanEditChange(const FProperty* InProperty) const override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

};
