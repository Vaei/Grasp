// Copyright (c) Jared Taylor

#pragma once

#include "CoreMinimal.h"
#include "GraspRequestTypes.h"
#include "Types/TargetingSystemTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "GraspSubsystem.generated.h"

/**
 * World subsystem providing spatial search for graspable components.
 * Mirrors USmartObjectSubsystem's FindSmartObjects API pattern for AI integration.
 *
 * Uses physics overlap queries against the Grasp collision channel/profile
 * to find graspable components, then filters by GraspData properties.
 */
UCLASS()
class GRASP_API UGraspSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Get the subsystem from any world context object. */
	static UGraspSubsystem* Get(const UObject* WorldContextObject);

	/**
	 * Find the single best graspable matching the request.
	 * @return Result for the closest matching graspable, or invalid result if none found.
	 */
	UFUNCTION(BlueprintCallable, Category = "Grasp", meta = (WorldContext = "WorldContextObject"))
	static FGraspRequestResult FindGraspable(const UObject* WorldContextObject, const FGraspRequest& Request);

	/**
	 * Find all graspables within the request's spatial query volume.
	 * @return True if any results were found.
	 */
	UFUNCTION(BlueprintCallable, Category = "Grasp", meta = (WorldContext = "WorldContextObject"))
	static bool FindGraspables(const UObject* WorldContextObject, const FGraspRequest& Request, TArray<FGraspRequestResult>& OutResults);

	/**
	 * Find graspable components on a specific list of actors.
	 * @return True if any results were found.
	 */
	UFUNCTION(BlueprintCallable, Category = "Grasp", meta = (WorldContext = "WorldContextObject"))
	static bool FindGraspablesInList(const UObject* WorldContextObject, const FGraspRequestFilter& Filter, const TArray<AActor*>& ActorList, TArray<FGraspRequestResult>& OutResults);

	/**
	 * Find graspable components from the results of a targeting request.
	 * @return True if any results were found.
	 */
	UFUNCTION(BlueprintCallable, Category = "Grasp", meta = (WorldContext = "WorldContextObject"))
	static bool FindGraspablesInTargetingRequest(const UObject* WorldContextObject, const FGraspRequestFilter& Filter, const FTargetingRequestHandle& TargetingHandle, TArray<FGraspRequestResult>& OutResults);

	/**
	 * Find all graspable components on a single actor.
	 * @return True if any results were found.
	 */
	UFUNCTION(BlueprintCallable, Category = "Grasp")
	static bool FindGraspablesOnActor(AActor* Actor, const FGraspRequestFilter& Filter, TArray<FGraspRequestResult>& OutResults);
};
