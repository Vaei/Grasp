// Copyright (c) Jared Taylor

#include "GraspSubsystem.h"
#include "GraspableComponent.h"
#include "GraspData.h"
#include "GraspDeveloper.h"
#include "GraspTypes.h"
#include "Abilities/GameplayAbility.h"
#include "CollisionQueryParams.h"
#include "Components/PrimitiveComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "TargetingSystem/TargetingSubsystem.h"
#include "Types/TargetingSystemTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GraspSubsystem)

namespace FGraspCVars
{
#if UE_ENABLE_DEBUG_DRAWING
	static int32 FindGraspablesDebug = 0;
	FAutoConsoleVariableRef CVarFindGraspablesDebug(
		TEXT("p.Grasp.FindGraspables.Debug"),
		FindGraspablesDebug,
		TEXT("Optionally draw debug for FindGraspables queries.\n")
		TEXT("0: Disable, 1: Draw query volume, 2: Draw query volume + result locations"),
		ECVF_Default);
#endif
}

UGraspSubsystem* UGraspSubsystem::Get(const UObject* WorldContextObject)
{
	if (const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr)
	{
		return World->GetSubsystem<UGraspSubsystem>();
	}
	return nullptr;
}

static FCollisionObjectQueryParams GetGraspCollisionQueryParams()
{
	const UGraspDeveloper* Settings = GetDefault<UGraspDeveloper>();
	FCollisionObjectQueryParams ObjectParams;

	if (Settings->GraspDefaultCollisionMode == EGraspDefaultCollisionMode::ObjectType)
	{
		ObjectParams.AddObjectTypesToQuery(Settings->GraspDefaultObjectType);
	}
	else
	{
		// Default: query the Grasp object channel (ECC_GameTraceChannel4)
		// This matches the project's Grasp collision channel
		ObjectParams.AddObjectTypesToQuery(ECC_GameTraceChannel4);
	}

	return ObjectParams;
}

static bool PassesFilter(const IGraspableComponent* Graspable, const UGraspData* GraspData, const FGraspRequestFilter& Filter)
{
	if (!GraspData)
	{
		return false;
	}

	if (!Filter.bIncludeDead && Graspable->IsGraspableDead())
	{
		return false;
	}

	if (!Filter.bIncludeWithoutAbility && !GraspData->GetGraspAbility())
	{
		return false;
	}

	if (Filter.GraspDataClass && !GraspData->IsA(Filter.GraspDataClass))
	{
		return false;
	}

	if (!Filter.TagRequirements.IsEmpty() && GraspData->InputTag.IsValid())
	{
		FGameplayTagContainer Tags;
		Tags.AddTag(GraspData->InputTag);
		if (!Filter.TagRequirements.Matches(Tags))
		{
			return false;
		}
	}
	else if (!Filter.TagRequirements.IsEmpty() && !GraspData->InputTag.IsValid())
	{
		// Filter requires tags but this GraspData has no InputTag
		return false;
	}

	return true;
}

static void FilterOverlapResults(
	const TArray<FOverlapResult>& OverlapResults,
	const FGraspRequestFilter& Filter,
	const FVector& QueryOrigin,
	TArray<FGraspRequestResult>& OutResults)
{
	for (const FOverlapResult& Overlap : OverlapResults)
	{
		UPrimitiveComponent* Component = Overlap.GetComponent();
		if (!Component || !Component->GetOwner())
		{
			continue;
		}

		if (Component->GetOwner()->IsPendingKillPending())
		{
			continue;
		}

		IGraspableComponent* Graspable = Cast<IGraspableComponent>(Component);
		if (!Graspable)
		{
			continue;
		}

		const int32 NumGraspData = Graspable->GetNumGraspData();
		for (int32 Index = 0; Index < NumGraspData; ++Index)
		{
			const UGraspData* GraspData = Graspable->GetGraspData(Index);
			if (PassesFilter(Graspable, GraspData, Filter))
			{
				const float Distance = FVector::Dist(QueryOrigin, Component->GetComponentLocation());
				OutResults.Emplace(Component, Index, Distance);
			}
		}
	}

	// Sort by distance
	OutResults.Sort([](const FGraspRequestResult& A, const FGraspRequestResult& B)
	{
		return A.Distance < B.Distance;
	});
}

FGraspRequestResult UGraspSubsystem::FindGraspable(const UObject* WorldContextObject, const FGraspRequest& Request)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UGraspSubsystem::FindGraspable);

	TArray<FGraspRequestResult> Results;
	if (FindGraspables(WorldContextObject, Request, Results) && Results.Num() > 0)
	{
		return Results[0];
	}
	return FGraspRequestResult();
}

bool UGraspSubsystem::FindGraspables(const UObject* WorldContextObject, const FGraspRequest& Request, TArray<FGraspRequestResult>& OutResults)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	if (!World)
	{
		return false;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(UGraspSubsystem::FindGraspables);

	OutResults.Reset();

	const FCollisionObjectQueryParams ObjectParams = GetGraspCollisionQueryParams();
	FCollisionQueryParams QueryParams;
	QueryParams.bReturnPhysicalMaterial = false;

	TArray<FOverlapResult> OverlapResults;

	if (Request.bUseSphere)
	{
		const FCollisionShape Shape = FCollisionShape::MakeSphere(Request.QuerySphereRadius);
		World->OverlapMultiByObjectType(OverlapResults, Request.QuerySphereCenter, FQuat::Identity, ObjectParams, Shape, QueryParams);
		FilterOverlapResults(OverlapResults, Request.Filter, Request.QuerySphereCenter, OutResults);

#if UE_ENABLE_DEBUG_DRAWING
		if (FGraspCVars::FindGraspablesDebug >= 1)
		{
			DrawDebugSphere(World, Request.QuerySphereCenter, Request.QuerySphereRadius, 16,
				OutResults.Num() > 0 ? FColor::Green : FColor::Red, false, 0.5f);
		}
#endif
	}
	else
	{
		const FVector Center = Request.QueryBox.GetCenter();
		const FVector Extent = Request.QueryBox.GetExtent();
		const FCollisionShape Shape = FCollisionShape::MakeBox(Extent);
		World->OverlapMultiByObjectType(OverlapResults, Center, FQuat::Identity, ObjectParams, Shape, QueryParams);
		FilterOverlapResults(OverlapResults, Request.Filter, Center, OutResults);

#if UE_ENABLE_DEBUG_DRAWING
		if (FGraspCVars::FindGraspablesDebug >= 1)
		{
			DrawDebugBox(World, Center, Extent, OutResults.Num() > 0 ? FColor::Green : FColor::Red, false, 0.5f);
		}
#endif
	}

#if UE_ENABLE_DEBUG_DRAWING
	if (FGraspCVars::FindGraspablesDebug >= 2)
	{
		for (const FGraspRequestResult& Result : OutResults)
		{
			if (const UPrimitiveComponent* Comp = Result.GraspableComponent.Get())
			{
				DrawDebugPoint(World, Comp->GetComponentLocation(), 12.f, FColor::Cyan, false, 0.5f);
				DrawDebugString(World, Comp->GetComponentLocation() + FVector(0, 0, 20),
					FString::Printf(TEXT("D:%.0f I:%d"), Result.Distance, Result.GraspDataIndex),
					nullptr, FColor::White, 0.5f, false);
			}
		}
	}
#endif

	return OutResults.Num() > 0;
}

bool UGraspSubsystem::FindGraspablesInList(const UObject* WorldContextObject, const FGraspRequestFilter& Filter, const TArray<AActor*>& ActorList, TArray<FGraspRequestResult>& OutResults)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UGraspSubsystem::FindGraspablesInList);

	OutResults.Reset();

	for (AActor* Actor : ActorList)
	{
		if (!Actor || Actor->IsPendingKillPending())
		{
			continue;
		}

		TArray<FGraspRequestResult> ActorResults;
		if (FindGraspablesOnActor(Actor, Filter, ActorResults))
		{
			OutResults.Append(ActorResults);
		}
	}

	return OutResults.Num() > 0;
}

bool UGraspSubsystem::FindGraspablesInTargetingRequest(const UObject* WorldContextObject, const FGraspRequestFilter& Filter, const FTargetingRequestHandle& TargetingHandle, TArray<FGraspRequestResult>& OutResults)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UGraspSubsystem::FindGraspablesInTargetingRequest);

	OutResults.Reset();

	UTargetingSubsystem* TargetingSubsystem = WorldContextObject ? UTargetingSubsystem::GetTargetingSubsystem(WorldContextObject) : nullptr;
	if (!TargetingSubsystem)
	{
		return false;
	}

	TArray<AActor*> Actors;
	TargetingSubsystem->GetTargetingResultsActors(TargetingHandle, Actors);

	return FindGraspablesInList(WorldContextObject, Filter, Actors, OutResults);
}

bool UGraspSubsystem::FindGraspablesOnActor(AActor* Actor, const FGraspRequestFilter& Filter, TArray<FGraspRequestResult>& OutResults)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UGraspSubsystem::FindGraspablesOnActor);

	OutResults.Reset();

	if (!Actor || Actor->IsPendingKillPending())
	{
		return false;
	}

	TArray<UPrimitiveComponent*> Components;
	Actor->GetComponents<UPrimitiveComponent>(Components);

	for (UPrimitiveComponent* Component : Components)
	{
		IGraspableComponent* Graspable = Cast<IGraspableComponent>(Component);
		if (!Graspable)
		{
			continue;
		}

		const int32 NumGraspData = Graspable->GetNumGraspData();
		for (int32 Index = 0; Index < NumGraspData; ++Index)
		{
			const UGraspData* GraspData = Graspable->GetGraspData(Index);
			if (PassesFilter(Graspable, GraspData, Filter))
			{
				OutResults.Emplace(Component, Index, 0.f);
			}
		}
	}

	return OutResults.Num() > 0;
}
