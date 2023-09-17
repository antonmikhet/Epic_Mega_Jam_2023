// Copyright Sam Bonifacio 2021. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TetherCableProperties.h"
#include "TetherGuideSplineComponent.h"
#include "GameFramework/Actor.h"
#include "Mesh/CableMeshGenerationCurveDescription.h"
#include "Simulation/TetherAsyncSimulationTask.h"
#include "Simulation/TetherSimulationModel.h"
#include "Misc/EngineVersionComparison.h"
#if !UE_VERSION_OLDER_THAN(5,0,0)
#include "UObject/ObjectSaveContext.h"
#endif

#if WITH_EDITOR
#include "Mesh/TetherAsyncMeshBuildTask.h"
#endif

#include "TetherCableActor.generated.h"

struct FTetherSimulationParams;
struct FTetherSimulationResultInfo;

DECLARE_MULTICAST_DELEGATE(FTetherCableSimulationUpdated);

UENUM()
enum EMeshBuildInstruction
{
	DoNotBuild,
	BuildIfModified,
	AlwaysBuild
};

/**
 * Tether's core Actor representing a cable in the world
 */
UCLASS(Blueprintable, hideCategories=(Physics,Activation))
class TETHER_API ATetherCableActor : public AActor
{
	GENERATED_BODY()

public:

	UPROPERTY(Category = "TetherProperties", EditAnywhere, BlueprintReadWrite)
	FTetherCableProperties CableProperties;

	/**
	 * Freeze the current simulation and mesh to prevent it updating, after which it can be moved around like a static mesh
	 * This is useful to stop the cable being changed accidentally when it's meant to be final
	 */
	UPROPERTY(Category = "TetherProperties", EditAnywhere, BlueprintReadWrite, meta = (NoResimulate))
	bool bLockCurrentState = false;

	UPROPERTY(Category = "TetherDebug", EditAnywhere, BlueprintReadWrite, meta = (NoResimulate))
	bool bSynchronousRealtime = false;

	UPROPERTY(Category = "TetherDebug", EditAnywhere, BlueprintReadWrite, meta = (NoResimulate, ClampMin = "0.01", UIMax = "2"))
	float RealtimeInEditorTimeDilation = 1.f;

	UPROPERTY(Category = "TetherDebug", EditAnywhere, BlueprintReadWrite, meta = (NoResimulate))
	bool bDebugVisualizeSimulationParticles = false;

	/* Can severely impact performance for cables with a low Simulation Particle Segment Length */
	UPROPERTY(Category = "TetherDebug", EditAnywhere, BlueprintReadWrite, meta = (NoResimulate))
	bool bDebugVisualizeSimulationCollision = false;

	UPROPERTY(Category = "TetherDebug", EditAnywhere, BlueprintReadWrite, meta = (NoResimulate))
	bool bDebugVisualizeBuiltStaticMeshPoints = false;

	UPROPERTY(Category = "TetherDebug", EditAnywhere, BlueprintReadWrite, meta = (NoResimulate))
	bool bDebugVisualizeContactPoints = false;

	UPROPERTY(Category = "TetherDebug", EditAnywhere, BlueprintReadWrite, meta = (NoResimulate))
	bool bDebugVisualizeComponentLocations = false;

	/**
	 * Shows start and end tangents of each segment.
	 * Note: The end tangent is shown reversed, to be distinguishable from the next segment's start tangent.
	 */
	UPROPERTY(Category = "TetherDebug", EditAnywhere, BlueprintReadWrite, meta = (NoResimulate))
	bool bDebugVisualizeTangents = false;

	/**
	 * Controls if the mesh for the cable is visible or not
	 */
	UPROPERTY(Category = "Rendering", EditAnywhere, BlueprintReadWrite, meta = (NoResimulate))
	bool bVisible = true;

	ATetherCableActor();

	UStaticMeshComponent* GetStaticMeshComponent() const { return StaticMeshComponent; }

#if WITH_EDITOR
	void SetEditing(bool bIsEditing);

	void HandlePropertyChanged(FPropertyChangedEvent& PropertyChangedEvent);

	void DumpCableInfo();

	void OnEditorEndObjectMovement(UObject& Obj);
#endif

protected:

	virtual bool ShouldTickIfViewportsOnly() const override { return true; }

	virtual void PostActorCreated() override;

	virtual void PostLoad() override;

	virtual void PostInitializeComponents() override;
	
	virtual void PostInitProperties() override;

	virtual void OnConstruction(const FTransform& Transform) override;
	
	virtual void BeginPlay() override;

	virtual void Tick(float DeltaTime) override;

#if UE_VERSION_OLDER_THAN(5,0,0)
	virtual void PreSave(const class ITargetPlatform* TargetPlatform) override;
#else
	virtual void PreSave(FObjectPreSaveContext ObjectSaveContext) override;
#endif

	virtual void PostDuplicate(bool bDuplicateForPIE) override;

#if WITH_EDITOR

	virtual bool CanEditChange(const FProperty* InProperty) const override;
	
	virtual void PostEditMove(bool bFinished) override;

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	virtual void PostEditUndo() override;
#endif

	virtual void Destroyed() override;

private:

	UPROPERTY()
	USceneComponent* SceneRoot;

	// Static mesh component containing the baked cable
	UPROPERTY(Category = "TetherProperties", VisibleAnywhere, BlueprintReadOnly, meta=(AllowPrivateAccess = "true"))
	UStaticMeshComponent* StaticMeshComponent;

	// UTC time that this cable was initially created in editor
	// Used to determine which cables should collide with which other cables, for deterministic interaction between multiple cables
	UPROPERTY(NonPIEDuplicateTransient)
	FDateTime TimeCreated;

#if WITH_EDITOR

	// Is the cable currently being placed in editor mode
	bool bEditing = false;

	// Allow transform calculation before ComponentToWorld is set
	FTransform GetCableRootTransform() const;
	FVector CableRootTransformPosition(FVector Position) const;
	FVector CableRootTransformVector(FVector Vector) const;
	
	void OnLightingBuildStarted();
	void EnsureUpToDate();
#endif


#pragma region Simulation

public:

#if WITH_EDITOR
	FTetherCableSimulationUpdated OnSimulationUpdated;
#endif

	UTetherGuideSplineComponent* GetGuideSplineComponent() const { return GuideSpline; }

	bool CanBeModified() const;
	
#if WITH_EDITOR

	FVector GetPointLocation(int32 PointIndex) const;
	void SetPointLocation(int32 PointIndex, FVector WorldLocation, bool bSimulateIfModified = true);
	void SetEndLocation(FVector EndLocation);

	// True if the endpoint was modified
	bool UpdatePreviewPoint(int32 Point, FVector WorldLocation, bool bSimulateIfModified = true);

	void AddSegmentSlack(int32 Segment, float DeltaSlack, bool bSimulateIfModified = true);

	void AddGuideSplinePoint(int32 PointIndex, FVector EndLocation);

	void RemovePreviewEndpoint();

	int32 GetNumGuideSplinePoints() const;

	int32 GetNumGuideSplineSegments() const;

	TArray<UTetherPointSegmentDefinition*> GetPointSegmentDefinitions() const;

	bool ShouldSimulateBefore(const ATetherCableActor* OtherCable) const;

	void UpdateAndRebuildModifiedSegments(bool bSynchronous = false, EMeshBuildInstruction BuildMesh = DoNotBuild, bool bSimulateIfModified = true);
	
	// Invalidate all segments, rebuild and run async simulation
	void InvalidateAndResimulate(bool bSynchronous = false, EMeshBuildInstruction BuildMesh = DoNotBuild);

	bool IsRunningAsyncSimulation() const { return CurrentSegmentsRunningAsyncSimulation.Num() > 0; }

	bool IsSimulationOutdated() const { return bSimulationOutdated;  }

	int32 GetNumSimulationSegments() const;
	TArray<FTetherSimulationSegment>& GetSimulationSegments();
    const TArray<FTetherSimulationSegment>& GetSimulationSegmentsConst() const;

	void SetRealtimeSimulating(bool bShouldRealtimeSimulate);
	bool IsRealtimeSimulating() const { return bRealtimeSimulating; }

	void CancelAsyncSimulation();

	const FTetherSimulationModel& GetSimulationModel() const { return ActiveSimulationModel; }

	bool SimulationModelHasAnyParticles() const { return ActiveSimulationModel.HasAnyParticles(); }
#endif

private:

	// Spline used to allow the user to direct the overall shape of the cable and segments before simulation
	UPROPERTY()
	UTetherGuideSplineComponent* GuideSpline;

	// The active simulation state for the cable
	// This is updated before and after the cable is simulated, and is used to create the mesh geometry
	UPROPERTY()
	FTetherSimulationModel ActiveSimulationModel;

	// Transform of the last complete simulation, because the transform of the active simulation can be in progress
	// UPROPERTY()
	// FTransform LastSimulationTransform;
	
#if WITH_EDITOR

	// Time left over from the last realtime simulation that should be added onto the next
	float RealtimeSimulationTimeRemainder = 0.f;

	// Indices of segments currently being simulated asynchronously
	TArray<int32> CurrentSegmentsRunningAsyncSimulation = {};

	bool bRealtimeSimulating = false;

	// Delegate function to call every time a realtime simulation completes
	FOnTetherAsyncSimulationCompleteDelegate RealtimeSimulationCompleteDelegate;
	
	FAsyncTaskExecuterWithAbort<FTetherAsyncSimulationTask>* CurrentSimulationTask = nullptr;

	// True if the simulation was left outdated e.g. update was cancelled by the user
	bool bSimulationOutdated = false;

	bool bGuideSplineModified = false;
	TArray<int32> PendingRemovedGuideSplinePoints = {};

	// If true, build and switch to static mesh after the next async simulation completes
	EMeshBuildInstruction BuildAfterCurrentSimulation = DoNotBuild;

	bool bBuildAfterNextSimulation = false;

	/**
	*  Updates the number of simulation segments to match the number of guide spline segments
	*  Returns true if modified
	*/
	bool UpdateSimulationSegmentNum();

	TArray<int32> FindModifiedSegments() const;

	bool InvalidateSegment(FTetherSimulationSegment& Segment);

	bool InvalidateSegments(TArray<int32> SegmentIndices);

	void InvalidateAllSegments();

	bool IsAnySegmentInvalidated() const;

	void ResimulateInvalidatedSegments(bool bSynchronous = false, EMeshBuildInstruction BuildMesh = DoNotBuild);

	void UpdateInvalidSimulationSegmentsSplineInfo(FTetherSimulationModel& Model);

	void ResetInvalidatedSegmentsSimulationState(FTetherSimulationModel& Model);

	void BuildParticlesForSegment(FTetherSimulationModel& Model, int32 SegmentIndex);

	bool PrepareForSimulation(FTetherSimulationModel& InitialModel, FTetherSimulationParams& Params);

	FTetherSimulationResultInfo PerformSimulation(FTetherSimulationModel& InitialModel, float DeltaTime, bool bIgnoreSegmentsSimulatingAsync, EMeshBuildInstruction BuildMesh, bool bVerboseLogging);

	bool PerformAsyncSimulation(FTetherSimulationModel& InitialModel, float DeltaTime, bool bOnlySimulateInvalidatedSegments, EMeshBuildInstruction BuildMesh, FOnTetherAsyncSimulationCompleteDelegate CompleteCallback = FOnTetherAsyncSimulationCompleteDelegate());

	void RealtimeSimulationComplete(const FTetherSimulationModel& SimulatedModel, const FTetherSimulationResultInfo& ResultInfo);

	FVector GetCableForce() const;

	void DrawDebugSimulationParticleCollision(float Lifetime);

	FVector GetGuideSplinePointWorldLocation(int32 PointIndex) const;

	struct FTetherSimulationParams MakeSimulationParams(FTetherSimulationModel& ModelForSimulation) const;

	// Called after any simulation process (synchronous or asynchronous) is completed
	//	@param	SimulatedModel	Resultant model of the simulation. If simulated asynchronously, this is the state before it was reconciled with the active simulation.
	void HandleSimulationComplete(const FTetherSimulationResultInfo& ResultInfo, const FTetherSimulationModel& SimulatedModel, const FTetherSimulationParams& Params, EMeshBuildInstruction BuildMesh, bool bVerboseLogging, bool bSynchronous = false);

	void BlockUntilSimulationComplete();
#endif
	
#pragma endregion Simulation

#pragma region Mesh

public:

#if WITH_EDITOR

	void SetMeshType(ECableMeshGenerationType Type);
	
	bool IsDynamicPreview() const;

	bool IsBuildingMesh() const { return bBuildingMesh; }
	
	/**
	* Ensures cable is on static visibility, first updating the mesh if the simulation state has changed
	* If the simulation is running asynchronously, will queue up and switch to static mesh after the simulation has returned, so result may not be immediate
	* Mesh building is slow and opens a progress bar, so don't call frequently while the mesh properties are updating
	* Returns true if build was started, false if queued or aborted
	*/
	bool QueueBuildAndShowStaticMesh(bool bForceEvenIfNotModified = false, bool bSynchronous = false);

	bool CanAutoBuild() const;

	void BuildMesh();

	bool IsMeshOutdated() const { return !IsStaticMeshUpToDate(); }

	void CancelAsyncMeshBuild();

	const FCableMeshGenerationCurveDescription* GetBuiltCurveDescription() const;
#endif

private:

	// Internal static mesh object to contain mesh data without having to generate and maintain an external asset
	// NonPIEDuplicateTransient - We don't want to copy the reference to the StaticMesh in the source actor when duplicating in editor, but we do want to copy it for PIE worlds
	UPROPERTY(NonPIEDuplicateTransient)
	UStaticMesh* StaticMesh;

	// Last set of parameters that have been built to static mesh
	// Tracked so that we know when the static mesh is dirty/invalidated, and so we don't rebuild it and invalidate static lighting when we don't need to
	// Not marked as transient - copied to the duplicated actor
	UPROPERTY()
	FCableMeshGenerationCurveDescription BuiltCurveDescriptionLocalSpaceSimplified;

	// Editor preview mesh which can be quickly updated without having to bake a static mesh
    UPROPERTY()
    class UTetherCableMeshComponent* DynamicPreviewMesh;

    UPROPERTY()
    UTetherMeshGenerator* MeshGenerator;

#if WITH_EDITOR

	bool bBuildingMesh = false;
	
	FAsyncTaskExecuterWithAbort<FTetherAsyncMeshBuildTask>* CurrentMeshBuildTask = nullptr;

	// True if mesh generator properties have been modified since last time the mesh was built
	bool bMeshPropertiesModified = false;

	void UpdateStaticMeshObjectProperties();

	FBasicMeshGenerationOptions GetPreviewMeshGenerationOptions() const;

	// Set mesh visibilities to show either the static mesh or dynamic preview mesh (only one should be visible at a time)
	//	@param	bUpdateDynamicMeshIfVisible	If true, and switching to dynamic visibility, update the dynamic mesh geometry from the current simulation
	void UpdateMeshVisibilities(bool bUseStaticMesh, bool bUpdateDynamicMeshIfVisible);

	// Set mesh visibilities conditionally depending on if the static mesh is up to date or not
	// If the static mesh is up to date, show it, otherwise show the dynamic preview instead
	//	@param	bUpdateDynamicMeshIfVisible	If true, and switching to dynamic visibility, update the dynamic mesh geometry from the current simulation
	void UpdateMeshVisibilitiesConditional(bool bUpdateDynamicMeshIfVisible);

	UTetherMeshGenerator* GetMeshGenerator() const;
	
	// Set up the internal static mesh object for the cable
	void CreateStaticMesh();

	bool IsStaticMeshUpToDate() const;

	void BuildStaticMesh(const FCableMeshGenerationCurveDescription& Params, FOnTetherAsyncMeshBuildCompleteDelegate Callback, bool bForceEvenIfNotModified = false, bool bSynchronous = false);

	bool CanSilentBuild() const;

	void HandleMeshGenerator(UTetherMeshGenerator* MeshGenerator);

	void BlockUntilMeshBuildComplete();

	// Returns mesh generation params in local space
	FCableMeshGenerationCurveDescription MakeMeshGenerationCurveDescriptionFromCurrentSimulationState(bool bSimplifyCurve) const;
#endif
	
#pragma endregion  Mesh
};
