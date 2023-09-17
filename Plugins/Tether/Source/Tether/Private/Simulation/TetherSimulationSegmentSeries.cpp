// Copyright Sam Bonifacio 2021. All Rights Reserved.

#include "Simulation/TetherSimulationSegmentSeries.h"

int32 FTetherSimulationSegmentSeries::GetNumSegments() const
{
    ensure(0);
    return -1;
}

FTetherSimulationSegment* FTetherSimulationSegmentSeries::GetSegment(int32 SegmentIndex)
{
    ensure(0);
    return nullptr;
}

const FTetherSimulationSegment* FTetherSimulationSegmentSeries::GetSegmentConst(int32 SegmentIndex) const
{
    ensure(0);
    return nullptr;
}

FTetherSimulationSegment* FTetherSimulationSegmentSeries::GetLastSegment()
{
    return GetSegment(GetNumSegments() - 1);
}

const FTetherSimulationSegment* FTetherSimulationSegmentSeries::GetLastSegmentConst() const
{
    return GetSegmentConst(GetNumSegments() - 1);
}

TArray<int32> FTetherSimulationSegmentSeries::GetSegmentUniqueIds() const
{
    TArray<int32> Result;
    for (int32 i=0 ; i < GetNumSegments(); i++)
    {
        const FTetherSimulationSegment* Segment = GetSegmentConst(i);
        Result.Add(Segment->SegmentUniqueId);
    }
    return Result;
}

int32 FTetherSimulationSegmentSeries::GetFirstSegmentUniqueId() const
{
    return GetSegmentConst(0)->SegmentUniqueId;
}

int32 FTetherSimulationSegmentSeries::IndexOfSegmentUniqueId(int32 SegmentUniqueId) const
{
	// Since we know segments are in order, index will be offset the same as unique ID
    return SegmentUniqueId - GetFirstSegmentUniqueId();
}

int32 FTetherSimulationSegmentSeries::GetNumParticles(bool bIncludeDuplicates) const
{
    if(GetNumSegments() == 0)
    {
        return 0;
    }
    int32 NumParticles = 0;
    for (int32 i=0 ; i < GetNumSegments(); i++)
    {
        const FTetherSimulationSegment* Segment = GetSegmentConst(i);
        const int32 SegmentNumParticles = Segment->GetNumParticles();
        if(SegmentNumParticles > 0)
        {
            if (NumParticles > 0 && !bIncludeDuplicates)
            {
                // If we already considered particles for a previous segment, account for duplication of shared endpoint particle
                NumParticles += FMath::Max(SegmentNumParticles - 1, 0);
            }
            else
            {
                NumParticles += Segment->GetNumParticles();
            }
        }
    }
    return NumParticles;
}

int32 FTetherSimulationSegmentSeries::GetStartingParticleIndexForSegment(int32 SegmentIndex) const
{
    int32 LastIndex = 0;
    for (int32 i=0; i< SegmentIndex ; i++)
    {
        const FTetherSimulationSegment* Segment = GetSegmentConst(i);
        const int32 SegmentNumParticles = Segment->GetNumParticles();
    	if(SegmentNumParticles > 0)
    	{
            LastIndex += Segment->GetNumParticles() - 1;
    	}
    }
    return LastIndex;
}

bool FTetherSimulationSegmentSeries::IsValidParticleIndex(int32 ParticleIndex) const
{
    return ParticleIndex >= 0 && ParticleIndex < GetNumParticles();
}

float FTetherSimulationSegmentSeries::GetLength() const
{
    float Length = 0.f;
    for (int32 i=0 ; i < GetNumSegments(); i++)
    {
        const FTetherSimulationSegment* Segment = GetSegmentConst(i);
        Length += Segment->Length;
    }
    return Length;
}

FSplineSegmentInfo FTetherSimulationSegmentSeries::GetSplineSegmentInfo() const
{
    FSplineSegmentInfo Info;
    Info.StartLocation = GetSegmentConst(0)->SplineSegmentInfo.StartLocation;
    Info.StartLeaveTangent = GetSegmentConst(0)->SplineSegmentInfo.StartLeaveTangent;
    Info.EndLocation = GetLastSegmentConst()->SplineSegmentInfo.EndLocation;
    Info.EndArriveTangent = GetLastSegmentConst()->SplineSegmentInfo.EndArriveTangent;
    return Info;
}

const FTetherSimulationSegment* FTetherSimulationSegmentSeries::GetSegmentWithParticle(int32 Index, bool bIncludeDuplicates) const
{
    check(Index >= 0);
    int32 CurrentSize = 0;
    for (int32 i = 0; i < GetNumSegments(); i++)
    {
        const FTetherSimulationSegment* Segment = GetSegmentConst(i);
        check(Segment);
        const int32 SegmentNumParticles = Segment->GetNumParticles();

        if (SegmentNumParticles > 0)
        {
            if (CurrentSize > 0 && !bIncludeDuplicates)
            {
                CurrentSize += FMath::Max(SegmentNumParticles - 1, 0);
            }
            else
            {
                CurrentSize += Segment->GetNumParticles();
            }
        }

        if (Index < CurrentSize)
        {
            // This segment has the particle we want
            return Segment;
        }
    }
    // Index out of range
    ensure(0);
    return GetLastSegmentConst();
}

FTetherSimulationParticle& FTetherSimulationSegmentSeries::GetParticle(int32 Index, bool bIncludeDuplicates)
{
    return const_cast<FTetherSimulationParticle&>(GetParticleConst(Index, bIncludeDuplicates));
}

const FTetherSimulationParticle& FTetherSimulationSegmentSeries::GetParticleConst(int32 Index, bool bIncludeDuplicates) const
{
    check(Index >= 0);
    int32 CurrentSize = 0;
    for (int32 i = 0; i < GetNumSegments(); i++)
    {
        const FTetherSimulationSegment* Segment = GetSegmentConst(i);
        check(Segment);
        const int32 SegmentNumParticles = Segment->GetNumParticles();

        if (SegmentNumParticles > 0)
        {
            if (CurrentSize > 0 && !bIncludeDuplicates)
            {
                CurrentSize += FMath::Max(SegmentNumParticles - 1, 0);
            }
            else
            {
                CurrentSize += Segment->GetNumParticles();
            }
        }
    	
        if (Index < CurrentSize)
        {
            // This segment has the particle we want, now calculate the index of the particle in the target segment
            const int32 DeltaToEnd = CurrentSize - Index;
            const int32 TargetSegmentParticleIndex = Segment->GetNumParticles() - DeltaToEnd;
            return Segment->Particles[TargetSegmentParticleIndex];
        }
    }
    // Index out of range
    ensure(0);
    return GetLastSegmentConst()->Particles.Last();
}

FTetherSimulationParticle& FTetherSimulationSegmentSeries::GetLastParticle()
{
    return GetParticle(GetNumParticles(false) - 1);
}

const FTetherSimulationParticle& FTetherSimulationSegmentSeries::GetLastParticleConst() const
{
    return GetParticleConst(GetNumParticles(false) - 1);
}

void FTetherSimulationSegmentSeries::SynchronizeConnectingParticles()
{
    for(int32 i=1;i<GetNumSegments();i++)
    {
        FTetherSimulationSegment* Segment = GetSegment(i);
        if(!ensure(Segment->Particles.IsValidIndex(0)))
        {
            return;
        }
        if(!ensure(Segment->Particles.IsValidIndex(0)))
        {
            return;
        }
        // Copy last particle of previous segment to first particle of this segment
        FTetherSimulationSegment* PreviousSegment = GetSegment(i - 1);
    	if(PreviousSegment->GetNumParticles() > 0)
    	{
            Segment->Particles[0] = PreviousSegment->Particles.Last();
    	}
    }
}

void FTetherSimulationSegmentSeries::AddSimulatedTime(float SimulatedTime)
{
    for(int32 i=0;i<GetNumSegments();i++)
    {
        FTetherSimulationSegment* Segment = GetSegment(i);
        Segment->SimulationTime += SimulatedTime;
    }
}

float FTetherSimulationSegmentSeries::GetSimulatedTime() const
{
	if(!ensure(GetNumSegments() > 0))
	{
        return 0.f;
	}
    return GetSegmentConst(0)->SimulationTime;
}

bool FTetherSimulationSegmentSeries::IsValid() const
{
    for(int32 i=0;i<GetNumSegments();i++)
    {
        if(!GetSegmentConst(i)->IsValid())
        {
            return false;
        }
    }
    return true;
}

int32 FTetherProxySimulationSegmentSeries::GetNumSegments() const
{
    return Segments.Num();
}

FTetherSimulationSegment* FTetherProxySimulationSegmentSeries::GetSegment(int32 SegmentIndex)
{
    return Segments[SegmentIndex];
}

const FTetherSimulationSegment* FTetherProxySimulationSegmentSeries::GetSegmentConst(int32 SegmentIndex) const
{
    return Segments[SegmentIndex];
}
