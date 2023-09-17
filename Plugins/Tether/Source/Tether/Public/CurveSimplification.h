// Copyright Sam Bonifacio 2021. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"

class TETHER_API FCurveSimplification
{
	
public:

	static TArray<FVector> SimplifyCurve(const TArray<FVector>& InPoints, float MaxDistance);
	
};
