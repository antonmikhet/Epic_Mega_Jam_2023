// Copyright Sam Bonifacio 2021. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/CollisionProfile.h"
#include "TetherCableSimulationOptions.generated.h"

USTRUCT(BlueprintType)
struct TETHER_API FTetherCableSimulationOptions
{
	GENERATED_BODY()

public:

	/**
	 *  The amount of time in seconds to simulate the cable asynchronously after the cable is modified.
	 *  Higher values increase stability of the final result, but also increase the time it takes to run the simulation whenever the cable is modified.
	 */
	UPROPERTY(Category = "TetherProperties", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", UIMax = "50.0"))
	float SimulationDuration = 5.f;
	
	/**
	 *  Controls the simulation substep time for the cable.
	 *  Lower numbers increase simulation accuracy but increase simulation time.
	 */
	UPROPERTY(Category = "TetherProperties", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.001", UIMin = "0.005", UIMax = "0.1"))
	float SubstepTime = 0.003f;


	/** Adds stiffness constraints to the cable simulation */
	UPROPERTY(Category = "TetherProperties", EditAnywhere, BlueprintReadWrite)
	bool bEnableStiffness = true;

	/** The number of solver iterations for constraints, controls how stiff the cable is */
	UPROPERTY(Category = "TetherProperties", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1", UIMax = "32", DisplayName="Stiffness"))
	int32 StiffnessSolverIterations = 4;

	/**
	 *  Damps velocity of cable particles, reducing cable sway
	 *  This may allow the cable to come to rest faster, therefore requiring less simulation time, at the cost of realism
	 */
	UPROPERTY(Category = "TetherProperties", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", UIMax = "1"))
	float Drag = 0.03f;

	/**
	 *	Perform sweeps for each cable particle, each substep, to avoid collisions with the world.
	 *	Greatly increases the simulation time.
	 *	This only causes the cable to collide with other objects while simulating. Once the cable is simulated, this does not control if the cable blocks other objects.
	 */
	UPROPERTY(Category = "TetherProperties", EditAnywhere, BlueprintReadWrite)
	bool bEnableCollision = true;

	/**
	 *  If true, the cable will collide with itself while simulating.
	 *  Adds further simulation time.
	 *  Requires CVar Tether.SelfCollision 1
	 */
	UPROPERTY(Category = "TetherProperties", EditAnywhere, BlueprintReadWrite, meta = (EditCondition = bEnableCollision))
	bool bEnableSelfCollision = true;

	/**
	 * Collision profile to use when simulating the cable, to control how it responds to other objects in the world
	 * This does not affect the collision of the final generated static mesh
	 */
	UPROPERTY(Category = "TetherProperties", EditAnywhere, BlueprintReadWrite, meta = (EditCondition = bEnableCollision))
	FCollisionProfileName CollisionProfile = UCollisionProfile::PhysicsActor_ProfileName;

	/** Scale to apply to the width of each particle to use for collision */
	UPROPERTY(Category = "TetherProperties", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.01", UIMin = "0.01", UIMax = "2.0", EditCondition = "bEnableCollision"))
	float CollisionWidthScale = 1.f;

	/** If collision is enabled, control how much sliding friction is applied when cable is in contact. */
	UPROPERTY(Category = "TetherProperties", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", ClampMax = "1.0", EditCondition = bEnableCollision))
	float CollisionFriction = 0.2f;

	/**
	*   Scale to apply to the desired distance between each particle for simulation
	*   Lower values create more particles, increasing simulation accuracy but also simulation time.
	*   Values above 1 may make collision with thin objects not work consistently, as the particles can separate and move around the object
	*/
	UPROPERTY(Category = "TetherProperties", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1.0", UIMax = "10"))
	float ParticleDistanceScale = 1.f;

	/**
	 * Over how much time in seconds from the start of the simulation to ease in constraints
	 * This can be used to artificially increase stability of the final result by reducing movement in the system early on when constraints are being applied to the initial state of the cable
	 * Should be less than the full simulation duration (e.g. half)
	 * The higher this is relative the the simulation duration, the more stiffness constraints will appear to lose their effect in the final result
	 */
	UPROPERTY(Category = "TetherProperties", EditAnywhere, BlueprintReadWrite)
	float ConstraintsEaseInTime = 2.5f;

	bool ShouldUseSelfCollision() const;
	void CheckSelfCollisionOptions() const;
};

FORCEINLINE uint32 GetTypeHash(const FTetherCableSimulationOptions& InOptions)
{
	uint32 Hash = GetTypeHash(InOptions.SimulationDuration);
	Hash = HashCombine(Hash, GetTypeHash(InOptions.SubstepTime));
	Hash = HashCombine(Hash, GetTypeHash(InOptions.bEnableStiffness));
	Hash = HashCombine(Hash, GetTypeHash(InOptions.StiffnessSolverIterations));
	Hash = HashCombine(Hash, GetTypeHash(InOptions.Drag));
	Hash = HashCombine(Hash, GetTypeHash(InOptions.bEnableCollision));
	Hash = HashCombine(Hash, GetTypeHash(InOptions.bEnableSelfCollision));
	Hash = HashCombine(Hash, GetTypeHash(InOptions.CollisionWidthScale));
	Hash = HashCombine(Hash, GetTypeHash(InOptions.CollisionFriction));
	Hash = HashCombine(Hash, GetTypeHash(InOptions.ParticleDistanceScale));
	Hash = HashCombine(Hash, GetTypeHash(InOptions.ConstraintsEaseInTime));
	return Hash;
}
