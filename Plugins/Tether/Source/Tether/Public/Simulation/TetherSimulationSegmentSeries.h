// Copyright Sam Bonifacio 2021. All Rights Reserved.

#pragma once

#include "TetherSimulationSegment.h"
#include "TetherSimulationSegmentSeries.generated.h"

/*
 * Interface to refer to and operate on a series of one or more connected cable segments, as if they were a single segment
 * It's assumed that the first particle of each segment contained shares the location of the last particle of the previous segment, thus effectively being joined
 */
USTRUCT()
struct TETHER_API FTetherSimulationSegmentSeries
{
    GENERATED_BODY()
 
    FTetherSimulationSegmentSeries(){};
    virtual ~FTetherSimulationSegmentSeries(){};

    virtual int32 GetNumSegments() const;
    virtual FTetherSimulationSegment* GetSegment(int32 SegmentIndex);
    virtual const FTetherSimulationSegment* GetSegmentConst(int32 SegmentIndex) const;

    FTetherSimulationSegment* GetLastSegment();
    const FTetherSimulationSegment* GetLastSegmentConst() const;

    TArray<int32> GetSegmentUniqueIds() const;

    int32 GetFirstSegmentUniqueId() const;
    int32 IndexOfSegmentUniqueId(int32 SegmentUniqueId) const;

    /*
     * Returns number of unique particles of all segments, not including the joining particles
     */
    int32 GetNumParticles(bool bIncludeDuplicates = false) const;

    bool HasAnyParticles() const { return GetNumParticles() > 0; }

    int32 GetNumParticleSegments() const { return GetNumParticles() - 1; }

    int32 GetStartingParticleIndexForSegment(int32 SegmentIndex) const;

    bool IsValidParticleIndex(int32 ParticleIndex) const;

    float GetLength() const;

    float GetParticleSegmentLength() const { return GetLength() / (float)GetNumParticleSegments(); }

    FSplineSegmentInfo GetSplineSegmentInfo() const;

    const FTetherSimulationSegment* GetSegmentWithParticle(int32 Index, bool bIncludeDuplicates = false) const;

    FTetherSimulationParticle& GetParticle(int32 Index, bool bIncludeDuplicates = false);
    const FTetherSimulationParticle& GetParticleConst(int32 Index, bool bIncludeDuplicates = false) const;

    FTetherSimulationParticle& GetLastParticle();
    const FTetherSimulationParticle& GetLastParticleConst() const;

    /*
     * Copies last particle of previous segment to first particle of next
     */
    void SynchronizeConnectingParticles();

    void AddSimulatedTime(float SimulatedTime);

    float GetSimulatedTime() const;

    virtual bool IsValid() const;

};

/**
 * Proxy implementation for SimulationSegmentSeries to operate on external segments
 */
USTRUCT()
struct TETHER_API FTetherProxySimulationSegmentSeries : public FTetherSimulationSegmentSeries
{
    GENERATED_BODY()
 
    /*
    * Actual simulation segments that make up this series
    */ 
    TArray<FTetherSimulationSegment*> Segments;

    virtual int32 GetNumSegments() const override;
    virtual FTetherSimulationSegment* GetSegment(int32 SegmentIndex) override;
    virtual const FTetherSimulationSegment* GetSegmentConst(int32 SegmentIndex) const override;
};
