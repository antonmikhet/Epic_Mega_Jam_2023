// Copyright Sam Bonifacio 2021. All Rights Reserved.

#include "CoreTypes.h"
#include "Misc/AutomationTest.h"
#include "Simulation/TetherSimulation.h"
#include "Simulation/TetherSimulationInstanceResources.h"
#include "Simulation/TetherSimulationModel.h"
#include "Simulation/TetherSimulationParams.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetherSimulationBasicTest, "Tether.Standard.Simulation.Basic Test", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTetherSimulationBasicTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Editor, false, FName(*GetTestName()), nullptr, false);
	World->CreatePhysicsScene();
	
	FTetherSimulationModel Model;
	Model.UpdateNumSegments(1);
	Model.Segments[0].SplineSegmentInfo.StartLocation = FVector::ZeroVector;
	Model.Segments[0].SplineSegmentInfo.EndLocation = FVector(1000.f, 0.f ,0.f);
	Model.Segments[0].Length = 1200.f;
	Model.Segments[0].BuildParticles(10.f);

	FTetherSimulationParams Params;
	Params.World = World;
	Params.SimulationOptions.SimulationDuration = Params.SimulationOptions.SubstepTime;
	
	FTetherSimulationInstanceResources Resources;
	Resources.InitializeResources(Model, Params);
	
	const FTetherSimulationResultInfo Result = FTetherSimulation::PerformSimulation(Model, 0.f, Params, nullptr);
	
	TestEqual(TEXT("Simulated time must equal substep time"), Result.SimulatedTime, Params.SimulationOptions.SubstepTime);

	World->DestroyWorld(false);
	
	return true;
}

void RunSimulationPerfTest(const FAutomationTestBase* Test, float SimulationDuration)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Editor, false, FName(*Test->GetTestName()), nullptr, false);
	World->CreatePhysicsScene();
	
	FTetherSimulationModel Model;
	Model.UpdateNumSegments(1);
	Model.Segments[0].SplineSegmentInfo.StartLocation = FVector::ZeroVector;
	Model.Segments[0].SplineSegmentInfo.EndLocation = FVector(1000.f, 0.f ,0.f);
	Model.Segments[0].Length = 1200.f;
	Model.Segments[0].BuildParticles(10.f);

	FTetherSimulationParams Params;
	Params.World = World;
	Params.SimulationOptions.SimulationDuration = SimulationDuration;
	
	FTetherSimulationInstanceResources Resources;
	Resources.InitializeResources(Model, Params);
	
	const FTetherSimulationResultInfo Result = FTetherSimulation::PerformSimulation(Model, 0.f, Params, nullptr);

	World->DestroyWorld(false);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetherSimulationPerformanceTest100Seconds, "Tether.Performance.Simulation.Performance Test 100 Seconds", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::PerfFilter)

bool FTetherSimulationPerformanceTest100Seconds::RunTest(const FString& Parameters)
{
	RunSimulationPerfTest(this, 100.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetherSimulationPerformanceTest500Seconds, "Tether.Performance.Simulation.Performance Test 500 Seconds", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::PerfFilter)

bool FTetherSimulationPerformanceTest500Seconds::RunTest(const FString& Parameters)
{
	RunSimulationPerfTest(this, 500.f);
	return true;
}

#endif
