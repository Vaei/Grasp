// Copyright (c) Jared Taylor

#pragma once

#include "CoreMinimal.h"

#include "GraspableComponent.generated.h"

struct FGameplayAbilityActorInfo;
struct FGameplayAbilityTargetData;
class UGraspData;

UINTERFACE()
class GRASP_API UGraspableComponent : public UInterface
{
	GENERATED_BODY()
};

/**
 * Inheritance band-aid, we want multiple derived UPrimitiveComponents to be able to implement this interface
 * This isn't intended for use outside the Grasp plugin
 * Implementation is assumed and never tested
 */
class GRASP_API IGraspableComponent
{
	GENERATED_BODY()

public:
	/**
	 * The GraspData for this component.
	 * This defines how we (the Pawn/Player/etc.) interact,
	 * as well as how the interactable behaves when interacted with.
	 *
	 * Includes parameters for adjusting the interaction distance, angle, height, etc.
	 */
	virtual const UGraspData* GetGraspData() const PURE_VIRTUAL(, return nullptr;);

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
	virtual bool IsGraspableDead() const { return false; }
};
