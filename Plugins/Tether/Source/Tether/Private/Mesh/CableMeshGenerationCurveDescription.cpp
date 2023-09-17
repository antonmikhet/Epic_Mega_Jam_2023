// Copyright Sam Bonifacio 2021. All Rights Reserved.

#include "Mesh/CableMeshGenerationCurveDescription.h"

FCableMeshGenerationCurveDescription::FCableMeshGenerationCurveDescription(const TArray<FVector>& PointLocations)
{
	for(const FVector& Location : PointLocations)
	{
		Points.Add(FCableMeshGenerationPoint(Location));
	}
}

void FCableMeshGenerationCurveDescription::CalcPointRotations()
{
	const int32 NumPoints = Points.Num();
	
	FVector LastForward;
	FVector LastUp;
	
	for (int32 PointIdx = 0; PointIdx < NumPoints; PointIdx++)
	{
		// Calculate forward direction for this point
		// by averaging previous and next points
		
		const int32 PrevIndex = FMath::Max(0, PointIdx - 1);
		const int32 NextIndex = FMath::Min(PointIdx + 1, NumPoints - 1);
		FVector ForwardDir;
		if (PointIdx == 0 && !StartTangent.IsZero())
		{
			ForwardDir = StartTangent.GetSafeNormal();
		}
		else if (PointIdx == NumPoints - 1 && !EndTangent.IsZero())
		{
			ForwardDir = EndTangent.GetSafeNormal();
		}
		else
		{
			ForwardDir = (Points[NextIndex].Location - Points[PrevIndex].Location).GetSafeNormal();
		}

		//FVector RightDir;
		FVector UpDir;
		if (PointIdx == 0)
		{
			// Find quat from up (Z) vector to forward
			const FQuat DeltaQuat = FQuat::FindBetween(FVector(0, 0, -1), ForwardDir);

			// Apply quat orth vectors
			//RightDir = DeltaQuat.RotateVector(FVector(0, 1, 0));
			UpDir = DeltaQuat.RotateVector(FVector(1, 0, 0));
		}
		else
		{
			// Continue the cable applying the same delta that was applied to the Forward vector since the last point
			FQuat ForwardDelta = FQuat::FindBetween(LastForward, ForwardDir);
			UpDir = ForwardDelta.RotateVector(LastUp);
			//RightDir = ForwardDir.RotateAngleAxis(90.f, UpDir);
		}

		LastForward = ForwardDir;
		LastUp = UpDir;

		Points[PointIdx].Rotation = FRotationMatrix::MakeFromXZ(ForwardDir, UpDir).ToQuat();
	}
}

TArray<FVector> FCableMeshGenerationCurveDescription::GetPointLocations() const
{
	TArray<FVector> Result;
	for(const FCableMeshGenerationPoint& Point : Points)
	{
		Result.Add(Point.Location);
	}
	return Result;
}

FCableMeshGenerationPointInfo FCableMeshGenerationCurveDescription::EvalPointInfo(float Key) const
{
	const int32 Left = FMath::FloorToInt(Key);
	const int32 Right = FMath::CeilToInt(Key);
	if(Left == Right)
	{
		return Points[Left].Info;
	}

	return FCableMeshGenerationPointInfo::Lerp(Points[Left].Info, Points[Right].Info, Key - (float)Left);
}
