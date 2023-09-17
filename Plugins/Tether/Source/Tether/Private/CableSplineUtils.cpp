// Copyright Sam Bonifacio 2021. All Rights Reserved.

#include "CableSplineUtils.h"

#include <DrawDebugHelpers.h>


#include "Components/SplineComponent.h"
#include "Mesh/CableMeshGenerationCurveDescription.h"

void FCableSplineUtils::CreateSplineFromPoints(FInterpCurveVector& Curve, TArray<FVector>& Points)
{
	Curve.Reset();
	for(int i=0;i<Points.Num();i++)
	{
		Curve.AddPoint(i, Points[i]);
		Curve.Points[i].InterpMode = EInterpCurveMode::CIM_CurveAuto;
	}
	Curve.AutoSetTangents(0.f, false);
}

TArray<FVector> FCableSplineUtils::CreatePointsFromSpline(FInterpCurveVector& Curve, int32 NumPoints)
{
	TArray<FVector> ResultPoints;
	if(Curve.Points.Num() == 0)
	{
		return ResultPoints;
	}
	const float Max = Curve.Points.Last().InVal;
	for(int i=0;i<NumPoints;i++)
	{
		const float PointFrac = (float)i / (float)(NumPoints - 1);
		const float CurveTime = PointFrac * Max;
		FVector Point = Curve.Eval(CurveTime, FVector::ZeroVector);
		ResultPoints.Add(Point);
	}
	return ResultPoints;
}

FSplineSegmentInfo FCableSplineUtils::SegmentInfoFromSplineComponent(USplineComponent* SplineComponent,
	int32 SegmentIndex, FTransform SplineWorldTransform)
{
	if(!ensure(SplineComponent))
	{
		return FSplineSegmentInfo();
	}

	const int32 EndPointIndex = SegmentIndex + 1;

	const bool bIsLastSegment = SegmentIndex == SplineComponent->GetNumberOfSplinePoints() - 2;
	const bool bHasEndPointTangent = !SplineComponent->GetArriveTangentAtSplinePoint(EndPointIndex, ESplineCoordinateSpace::Local).IsZero();
	
	FSplineSegmentInfo Result;
	Result.StartLocation = SplineWorldTransform.TransformPosition(SplineComponent->GetLocationAtSplineInputKey(SegmentIndex, ESplineCoordinateSpace::Local));
	Result.StartLeaveTangent = SplineWorldTransform.TransformVector(SplineComponent->GetLeaveTangentAtSplinePoint(SegmentIndex, ESplineCoordinateSpace::Local));
	Result.EndLocation = SplineWorldTransform.TransformPosition(SplineComponent->GetLocationAtSplineInputKey(EndPointIndex, ESplineCoordinateSpace::Local));
	if(SplineComponent->GetSplinePointType(SegmentIndex) == ESplinePointType::Linear)
	{
		// For linear segments, end tangent is the same as start
		// This doesn't appear to be the case for tangents provided by the spline component
		Result.EndArriveTangent = Result.StartLeaveTangent;
	}
	else
	{
		// For curved segments, get the end tangent from the spline component
		// Hack: If the spline point at the end if this segment is zero, use the tangent from the curve just before the end so that there will still be something useful, otherwise it will just be zero vector which can be unexpected
		const float EndKey = bIsLastSegment && !bHasEndPointTangent ? EndPointIndex - KINDA_SMALL_NUMBER : EndPointIndex;
		Result.EndArriveTangent = SplineWorldTransform.TransformVector(SplineComponent->GetTangentAtSplineInputKey(EndKey, ESplineCoordinateSpace::Local));
	}
	return Result;
}

FInterpCurveVector FCableSplineUtils::SplineFromSegmentInfo(const FSplineSegmentInfo& SegmentInfo)
{
	FInterpCurveVector Curve;
	ensure(Curve.Points.Num() == 0);
	
	Curve.AddPoint(0, SegmentInfo.StartLocation);
	Curve.AddPoint(1, SegmentInfo.EndLocation);

	Curve.Points[0].ArriveTangent = SegmentInfo.StartLeaveTangent;
	Curve.Points[0].LeaveTangent = SegmentInfo.StartLeaveTangent;
	Curve.Points[0].InterpMode = SegmentInfo.StartLeaveTangent.IsZero() ? EInterpCurveMode::CIM_Linear : EInterpCurveMode::CIM_CurveUser;

	Curve.Points[1].ArriveTangent = SegmentInfo.EndArriveTangent;
	Curve.Points[1].LeaveTangent = SegmentInfo.EndArriveTangent;
	Curve.Points[1].InterpMode = SegmentInfo.EndArriveTangent.IsZero() ? EInterpCurveMode::CIM_Linear : EInterpCurveMode::CIM_CurveUser;

	return Curve;
}

FInterpCurveFloat FCableSplineUtils::ReparamTableFromSplineCurve(const FInterpCurveVector& SplineCurve, int32 ReparamStepsPerSegment)
{
	const int32 NumSegments = SplineCurve.Points.Num() - 1;
	const bool bClosedLoop = false;
	const FVector Scale3D = FVector(1.0f);

	// Logic below taken from SplineComponent.cpp
	
	FInterpCurveFloat ReparamTable;
	ReparamTable.Points.Reset(NumSegments * ReparamStepsPerSegment + 1);
	float AccumulatedLength = 0.0f;
	for (int32 SegmentIndex = 0; SegmentIndex < NumSegments; ++SegmentIndex)
	{
		for (int32 Step = 0; Step < ReparamStepsPerSegment; ++Step)
		{
			const float Param = static_cast<float>(Step) / ReparamStepsPerSegment;
			const float SegmentLength = (Step == 0) ? 0.0f : FCableSplineUtils::GetSplineSegmentLength(SplineCurve, SegmentIndex, Param, bClosedLoop, Scale3D);

			ReparamTable.Points.Emplace(SegmentLength + AccumulatedLength, SegmentIndex + Param, 0.0f, 0.0f, CIM_Linear);
		}
		AccumulatedLength += FCableSplineUtils::GetSplineSegmentLength(SplineCurve, SegmentIndex, 1.0f, bClosedLoop, Scale3D);
	}

	ReparamTable.Points.Emplace(AccumulatedLength, static_cast<float>(NumSegments), 0.0f, 0.0f, CIM_Linear);

	return ReparamTable;
}

float FCableSplineUtils::GetSplineSegmentLength(const FInterpCurveVector& SplineCurve, const int32 Index, const float Param, bool bClosedLoop, const FVector& Scale3D)
{
	const FInterpCurveVector& Position = SplineCurve;

	// Logic below taken from SplineComponent.cpp
	
	const int32 NumPoints = Position.Points.Num();
	const int32 LastPoint = NumPoints - 1;

	check(Index >= 0 && ((bClosedLoop && Index < NumPoints) || (!bClosedLoop && Index < LastPoint)));
	check(Param >= 0.0f && Param <= 1.0f);

	// Evaluate the length of a Hermite spline segment.
	// This calculates the integral of |dP/dt| dt, where P(t) is the spline equation with components (x(t), y(t), z(t)).
	// This isn't solvable analytically, so we use a numerical method (Legendre-Gauss quadrature) which performs very well
	// with functions of this type, even with very few samples.  In this case, just 5 samples is sufficient to yield a
	// reasonable result.

	struct FLegendreGaussCoefficient
	{
		float Abscissa;
		float Weight;
	};

	static const FLegendreGaussCoefficient LegendreGaussCoefficients[] =
	{
		{ 0.0f, 0.5688889f },
		{ -0.5384693f, 0.47862867f },
		{ 0.5384693f, 0.47862867f },
		{ -0.90617985f, 0.23692688f },
		{ 0.90617985f, 0.23692688f }
	};

	const auto& StartPoint = Position.Points[Index];
	const auto& EndPoint = Position.Points[Index == LastPoint ? 0 : Index + 1];

	const auto& P0 = StartPoint.OutVal;
	const auto& T0 = StartPoint.LeaveTangent;
	const auto& P1 = EndPoint.OutVal;
	const auto& T1 = EndPoint.ArriveTangent;

	// Special cases for linear or constant segments
	if (StartPoint.InterpMode == CIM_Linear)
	{
		return ((P1 - P0) * Scale3D).Size() * Param;
	}
	else if (StartPoint.InterpMode == CIM_Constant)
	{
		return 0.0f;
	}

	// Cache the coefficients to be fed into the function to calculate the spline derivative at each sample point as they are constant.
	const FVector Coeff1 = ((P0 - P1) * 2.0f + T0 + T1) * 3.0f;
	const FVector Coeff2 = (P1 - P0) * 6.0f - T0 * 4.0f - T1 * 2.0f;
	const FVector Coeff3 = T0;

	const float HalfParam = Param * 0.5f;

	float Length = 0.0f;
	for (const auto& LegendreGaussCoefficient : LegendreGaussCoefficients)
	{
		// Calculate derivative at each Legendre-Gauss sample, and perform a weighted sum
		const float Alpha = HalfParam * (1.0f + LegendreGaussCoefficient.Abscissa);
		const FVector Derivative = ((Coeff1 * Alpha + Coeff2) * Alpha + Coeff3) * Scale3D;
		Length += Derivative.Size() * LegendreGaussCoefficient.Weight;
	}
	Length *= HalfParam;

	return Length;
}

TArray<FVector> FCableSplineUtils::WorldPointsToLocal(const TArray<FVector>& WorldPoints, FTransform WorldTransform)
{
	TArray<FVector> Result;
	for(const FVector& WorldPoint : WorldPoints)
	{
		Result.Add(WorldTransform.InverseTransformPosition(WorldPoint));
	}
	return Result;
}

TArray<FVector> FCableSplineUtils::LocalPointsToWorld(const TArray<FVector>& LocalPoints, FTransform WorldTransform)
{
	TArray<FVector> Result;
	for (const FVector& LocalPoint : LocalPoints)
	{
		Result.Add(WorldTransform.TransformPosition(LocalPoint));
	}
	return Result;
}

TArray<bool> FCableSplineUtils::FindContactPoints(const UWorld* World, const TArray<FVector>& WorldPoints, float CheckRadius, const TArray<const AActor*> IgnoreActors)
{
	TArray<bool> PointHits;
	FCollisionQueryParams Params;
	Params.AddIgnoredActors(IgnoreActors);
	Params.bTraceComplex = true;
	for(const FVector& WorldPoint: WorldPoints)
	{
		const bool bHit = World->OverlapBlockingTestByChannel(WorldPoint, FQuat::Identity, ECollisionChannel::ECC_PhysicsBody, FCollisionShape::MakeSphere(CheckRadius), Params);
		//::DrawDebugSphere(World, WorldPoint, CheckRadius, 8, bHit? FColor::Red : FColor(255,255,255,128), true, 4.f, SDPG_Foreground, 1.f);
		PointHits.Add(bHit);
	}
	return PointHits;
}

void FCableSplineUtils::CalculatePointInfo(const UWorld* World, const TArray<FVector>& WorldPoints, float CheckRadius, const TArray<const AActor*> IgnoreActors, TArray<FCableMeshGenerationPoint>& PointInfos)
{
	ensure(WorldPoints.Num() == PointInfos.Num());
	TArray<bool> ContactPoints = FindContactPoints(World, WorldPoints, CheckRadius, IgnoreActors);
	ensure(ContactPoints.Num() == WorldPoints.Num());
	ContactPoints[0] = true;
	ContactPoints.Last() = true;
	const float TotalLength = CalculateLength(WorldPoints);
	for(int32 PointIdx = 0; PointIdx< WorldPoints.Num(); PointIdx++)
	{
		FCableMeshGenerationPoint& PointInfo = PointInfos[PointIdx];
		if(ContactPoints[PointIdx])
		{
			// This point itself is a contact point, so distance is 0
			continue;
		}

		int32 LeftContactPoint = -1;
		float LeftDistance = 0.f;

		// Traverse backward and try to find nearest left-side contact point
		for(int32 LeftIdx = PointIdx - 1; LeftIdx >= 0; LeftIdx-- )
		{
			LeftDistance += FVector::Dist(WorldPoints[LeftIdx], WorldPoints[LeftIdx + 1]);
			if(ContactPoints[LeftIdx])
			{
				LeftContactPoint = LeftIdx;
				break;
			}
		}

		int32 RightContactPoint = -1;
		float RightDistance = 0.f;

		// Do the same for right side
		for(int32 RightIdx = PointIdx + 1; RightIdx < WorldPoints.Num(); RightIdx++)
		{
			RightDistance += FVector::Dist(WorldPoints[RightIdx - 1], WorldPoints[RightIdx]);
			if(ContactPoints[RightIdx])
			{
				RightContactPoint = RightIdx;
				break;
			}
		}

		const float SegmentLength = LeftDistance + RightDistance;
		PointInfo.Info.SegmentLength = SegmentLength;
		const float SegmentCableLengthRatio = SegmentLength / TotalLength;

		// The distance of the point to the nearest contact point as a percentage of the total length of the cable
		// For example if a hanging cable is forming a perfect catenary with a contact point on each end, a point perfectly in the middle will have 0.5 distance to the nearest contact point, i.e. half the total length of the cable
		PointInfo.Info.DistanceToNearestContactPoint = FMath::Min(LeftDistance, RightDistance) / TotalLength;

		const float SegmentLengthRatio = LeftDistance / SegmentLength;
		const float Hang = -1.f * FMath::Square(2.f * SegmentLengthRatio - 1.f) + 1.f; // Quadratic
		const float SegmentLooseness = FMath::Pow(Hang, 3);
		const float CableRelativeLooseness = SegmentCableLengthRatio * SegmentLooseness;
		PointInfo.Info.Looseness = CableRelativeLooseness;

		const float DistanceToSegmentLine = FMath::PointDistToSegment(WorldPoints[PointIdx], WorldPoints[LeftContactPoint], WorldPoints[RightContactPoint]);
		const float CableRelativeDistance = SegmentCableLengthRatio * DistanceToSegmentLine;
		PointInfo.Info.DistanceToSegmentLine = CableRelativeDistance;

		const float SegmentLineDistance = FVector::Distance(WorldPoints[LeftContactPoint], WorldPoints[RightContactPoint]);
		PointInfo.Info.SegmentLineDistance = SegmentLineDistance;
		const float PointSlackRatio = SegmentLength / SegmentLineDistance;
		PointInfo.Info.SlackRatio = PointSlackRatio;
	}
}

float FCableSplineUtils::CalculateLength(const TArray<FVector>& Points)
{
	float Length = 0.f;
	for(int32 i=0; i< Points.Num() -1; i++)
	{
		Length += FVector::Dist(Points[i], Points[i + 1]);
	}
	return Length;
}
