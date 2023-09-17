// Copyright Sam Bonifacio 2021. All Rights Reserved.

#include "Simulation/TetherSimulationInstanceResources.h"
#include "Simulation/TetherSimulationParams.h"
#include "CoreMinimal.h"
#include "PhysicsEngine/BodySetup.h"
#include "Simulation/TetherPhysicsUtils.h"
#include "Simulation/TetherSimulationModel.h"

FTetherSimulationInstanceResources::FTetherSimulationInstanceResources()
{
}

FTetherSimulationInstanceResources::~FTetherSimulationInstanceResources()
{
    ReleaseResources();
}

void FTetherSimulationInstanceResources::InitializeResources(FTetherSimulationModel& Model, FTetherSimulationParams& Params)
{
    check(IsInGameThread());

    // Body setup and instances are only needed for self-collision
    if(Params.SimulationOptions.ShouldUseSelfCollision())
    {
        // Initialize Body Setup for this simulation instance
        BodySetup = NewObject<UBodySetup>(GetTransientPackage(), NAME_None, RF_Transient);
        BodySetup->AddToRoot();
        BodySetup->AggGeom.SphereElems.Add(FKSphereElem(0.5f * Params.CollisionWidth));
        BodySetup->CollisionReponse = EBodyCollisionResponse::BodyCollision_Enabled;
        BodySetup->CollisionTraceFlag = CTF_UseSimpleAsComplex;
        BodySetup->bNeverNeedsCookedCollisionData = true;

        // Initialize the Body Instances
        if(!ensure(Params.World.IsValid()))
        {
            return;
        }

        World = Params.World;
    
        FPhysScene* PhysScene = Params.World->GetPhysicsScene();
        ensure(PhysScene);

    	// Create bodies for all particles up until and inclusive of those being simulated (but not any particles after)

        const TArray<FTetherProxySimulationSegmentSeries> SimulationSeries = Params.MakeSegmentSeriesToSimulate(Model);

    	if(SimulationSeries.Num() > 0)
		{
            const int32 LastSegmentToSimulate = SimulationSeries.Last().Segments.Last()->SegmentUniqueId;

            const FTetherProxySimulationSegmentSeries BodyProxySeries = Model.MakeSeriesUpTo(LastSegmentToSimulate);

            const int32 NumParticles = BodyProxySeries.GetNumParticles();
            TArray<FTransform> Transforms;

            if (ensure(NumParticles > 0))
            {
                BodyInstances = {};
                BodyInstances.SetNumUninitialized(NumParticles);
                BodyInstances.Reserve(NumParticles);

                for (int32 i = 0; i < NumParticles; i++)
                {
                    const FTetherSimulationParticle& Particle = BodyProxySeries.GetParticleConst(i);
                    FBodyInstance* Instance = new FBodyInstance;
                    BodyInstances[i] = Instance;
                    //BodyInstances.Add(Instance);
                    Instance->InstanceBodyIndex = i;
                    Instance->bAutoWeld = false;
                    Instance->bSimulatePhysics = false;
                    Instance->BodySetup = BodySetup;
                    Instance->OwnerComponent = Params.Component;
                    Instance->SetCollisionEnabled(ECollisionEnabled::Type::QueryOnly);
                    Instance->SetResponseToChannel(ECollisionChannel::ECC_Visibility, ECollisionResponse::ECR_Ignore);
                    Transforms.Add(FTransform(Particle.Position));
                }
            }

            check(BodyInstances.Num() > 0);
            check(BodyInstances.Num() == NumParticles);
            check(Transforms.Num() == NumParticles);
            FBodyInstance::InitStaticBodies(BodyInstances, Transforms, BodySetup, Params.Component.Get(), PhysScene);

            // Set BodyInstances on params
            check(Params.BodyInstances.Num() == 0);
            Params.BodyInstances = BodyInstances;
		}

        FTetherPhysicsUtils::CheckBodyInstances(Params.BodyInstances);
    }

    bIsInitialized = true;
}

void FTetherSimulationInstanceResources::ReleaseResources()
{
    check(IsInGameThread());

    // This should be no-op safe and fine to call multiple times
    if(!bIsInitialized)
    {
        return;
    }
    
    // Terminate body instances
    for (FBodyInstance* Instance : BodyInstances)
    {
        if(Instance)
        {
        	if(World.IsValid())
        	{
                Instance->TermBody(true);
        	}
            delete Instance;
        }
    }
    BodyInstances.Reset();

    // Destroy body setup
    if(IsValid(BodySetup))
    {
        BodySetup->ConditionalBeginDestroy();
        BodySetup->RemoveFromRoot();
        BodySetup = nullptr;
    }

    bIsInitialized = false;
}
