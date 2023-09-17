// Copyright Sam Bonifacio 2021. All Rights Reserved.

#include "Simulation/TetherPhysicsUtils.h"
#include "Engine/World.h"
#include "PhysicsInterfaceDeclaresCore.h"
#include "PhysicsEngine/BodyInstance.h"

#if PHYSICS_INTERFACE_PHYSX
#include "PhysXPublicCore.h"
#include <PxScene.h>
#include "PxRigidActor.h"
#include "Physics/PhysScene_PhysX.h"
#endif

#if PHYSICS_INTERFACE_PHYSX
uint32 HashPxActor(PxActor* Actor)
{
	PxRigidActor* RigidActor = (PxRigidActor*)Actor;
	const FTransform Transform = RigidActor ? P2UTransform(RigidActor->getGlobalPose()) : FTransform::Identity;
	uint32 Hash = GetTypeHash(Transform.GetLocation());
	Hash = HashCombine(Hash, GetTypeHash(Transform.GetRotation().Euler()));
	Hash = HashCombine(Hash, GetTypeHash(Transform.GetScale3D()));
	return Hash;
}
#endif

uint32 FTetherPhysicsUtils::HashPhyiscsBodies(UWorld* World)
{
	FPhysScene* PhysScene = World->GetPhysicsScene();
	uint32 Hash;
	const bool bSuccess = FPhysicsCommand::ExecuteRead(PhysScene, [&Hash, PhysScene]()
	{
// #if PHYSICS_INTERFACE_PHYSX
// 		const uint32 NumStatic = PhysScene->GetPxScene()->getNbActors(PxActorTypeFlag::eRIGID_STATIC);
// 		const uint32 NumDynamic = PhysScene->GetPxScene()->getNbActors(PxActorTypeFlag::eRIGID_DYNAMIC);
// 		const uint32 NumActors = NumStatic + NumDynamic;
//
// 		TArray<PxActor*> Actors;
// 		Actors.AddUninitialized(NumActors);
// 		if (NumStatic)
// 		{
// 		    PhysScene->GetPxScene()->getActors(PxActorTypeFlag::eRIGID_STATIC, Actors.GetData(), NumStatic);
// 		}
//
// 		if (NumDynamic)
// 		{
// 		    PhysScene->GetPxScene()->getActors(PxActorTypeFlag::eRIGID_DYNAMIC, &Actors[NumStatic], NumDynamic);
// 		}
//
// 		Hash = GetTypeHash(NumActors);
// 		for(PxActor* Actor : Actors)
// 		{
// 		    Hash = HashCombine(Hash, HashPxActor(Actor));
// 		}
// #endif
	});
	check(bSuccess);
	return Hash;
}

void FTetherPhysicsUtils::CheckBodyInstances(const TArray<FBodyInstance*>& BodyInstances)
{
	for(FBodyInstance* BodyInstance : BodyInstances)
	{
		check(BodyInstance->IsValidBodyInstance());
		check(FPhysicsInterface::IsValid(BodyInstance->GetPhysicsActorHandle()));
	}
}
