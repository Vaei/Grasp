// Copyright (c) Jared Taylor

#pragma once

#include "CoreMinimal.h"

#include "GraspableOwner.generated.h"

struct FGameplayAbilityActorInfo;
struct FGameplayAbilityTargetData;
class UGraspData;

UINTERFACE()
class GRASP_API UGraspableOwner : public UInterface
{
	GENERATED_BODY()
};

/**
 * Optional interface to add to the owner of a GraspableComponent
 */
class GRASP_API IGraspableOwner
{
	GENERATED_BODY()

public:
	/** 
	 * Optional additional target data that will be passed to the ability when the graspable is interacted with.
	 * @return The optional target data for this graspable.
	 */
	virtual TArray<FGameplayAbilityTargetData*> GatherOptionalGraspTargetData(const FGameplayAbilityActorInfo* ActorInfo) const
	{
		return {};
	}
	
	/**
	 * Dead graspables have their abilities removed from the Pawn that they were granted to.
	 * 
	 * If the graspable becomes available again in the future and is interacted with immediately after,
	 * before the ability is re-granted -- there will be de-sync.
	 *
	 * You do not need to check IsPendingKillPending() or IsTornOff() on the owner, this is done for you.
	 * 
	 * @return True if this graspable is no longer available, e.g. a Barrel that is exploding, a Pawn who is dying.
	 */
	UFUNCTION(BlueprintNativeEvent, Category=Grasp)
	bool IsGraspableDead() const;
	virtual bool IsGraspableDead_Implementation() const { return false;	}
};
