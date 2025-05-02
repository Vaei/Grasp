// Copyright (c) Jared Taylor

#pragma once

#include "CoreMinimal.h"
#include "GameplayAbilitySpecHandle.h"
#include "Abilities/GameplayAbility.h"
#include "GraspAbilityData.generated.h"

/**
 * Granted ability data
 */
USTRUCT()
struct GRASP_API FGraspAbilityData
{
	GENERATED_BODY()

	FGraspAbilityData()
		: bPersistent(false)
		, Handle(FGameplayAbilitySpecHandle())
		, Ability(nullptr)
	{}

	UPROPERTY()
	bool bPersistent;

	UPROPERTY()
	FGameplayAbilitySpecHandle Handle;

	UPROPERTY()
	TSubclassOf<UGameplayAbility> Ability;

	/** Interactables that are in range and require this ability remain active */
	UPROPERTY()
	TArray<TWeakObjectPtr<const UPrimitiveComponent>> Graspables;
};