// Copyright (c) Jared Taylor

#pragma once

#include "CoreMinimal.h"
#include "GameplayAbilitySpecHandle.h"
#include "GameplayTagContainer.h"
#include "GraspTypes.generated.h"

struct FGameplayAbilitySpec;
class UAbilitySystemComponent;
class IGraspableComponent;
class UGameplayAbility;
class UGraspData;
class UGraspComponent;

DECLARE_LOG_CATEGORY_EXTERN(LogGrasp, Log, All);

DECLARE_DELEGATE_OneParam(FOnPauseGrasp, bool /* bIsPaused */);
DECLARE_DELEGATE(FOnRequestGrasp);

UENUM(BlueprintType)
enum class EGraspTargetingSource : uint8
{
	Pawn						UMETA(ToolTip="Use the controlled Pawn as the targeting source"),
	PawnIfValid					UMETA(ToolTip="Use the controlled Pawn as the targeting source, or the Controller if invalid"),
	Controller					UMETA(ToolTip="Use the Controller as the targeting source"),
};

/**
 * Query what an interactor can do with a graspable component
 */
UENUM(BlueprintType)
enum class EGraspQueryResult : uint8
{
	None			UMETA(ToolTip="Cannot interact or highlight"),
	Highlight		UMETA(ToolTip="Highlight, but not interact -- typically for driving UI"),
	Interact		UMETA(ToolTip="Can interact"),
};

/**
 * Focus handling for the Grasp system
 * Not implemented by default but common enough that it should be here
 */
UENUM(BlueprintType)
enum class EGraspFocusMode : uint8
{
	None 			UMETA(ToolTip="Does not require focus to interact"),
	Focus			UMETA(ToolTip="Requires focus to interact"),
	FocusAlways		UMETA(ToolTip="Requires focus to interact and ability will end if focus is lost"),
};

/**
 * How Grasp abilities retrieve their GraspableComponent
 * Determine what checks are done from the ability
 */
UENUM(BlueprintType)
enum class EGraspAbilityComponentSource : uint8
{
	EventData		UMETA(ToolTip="Send the GraspableComponent along with the event data. Results in ShouldAbilityRespondToEvent() and ActivateAbilityFromEvent()"),
	Automatic		UMETA(ToolTip="Send EventData if GraspableComponent has optional target data. @see IGraspable::GatherOptionalGraspTargetData()"),
	Custom			UMETA(ToolTip="Unimplemented -- use a focus system or similar to determine which GraspableComponent we're interacting with. Results in ActivateAbility()"),
};

UENUM(BlueprintType)
enum class EGraspCardinal_4Way : uint8
{
	Forward,
	Left,
	Right,
	Backward,
};

UENUM(BlueprintType)
enum class EGraspCardinal_8Way : uint8
{
	Forward,
	ForwardLeft,
	ForwardRight,
	Left,
	Right,
	Backward,
	BackwardLeft,
	BackwardRight,
};

UENUM(BlueprintType)
enum class EGraspCardinalType : uint8
{
	Cardinal_4Way,
	Cardinal_8Way,
};

/**
 * Grasp will scan for interactables to retrieve their data and ability
 */
USTRUCT()
struct GRASP_API FGraspScanResult
{
	GENERATED_BODY()

	FGraspScanResult(const FGameplayTag& InScanTag = FGameplayTag::EmptyTag,
		const TWeakObjectPtr<const UPrimitiveComponent>& InGraspable = nullptr,
		float InNormalizedAvatarDistance = 0.f)
		: ScanTag(InScanTag)
		, Graspable(InGraspable)
		, NormalizedScanDistance(InNormalizedAvatarDistance)
	{}

	/** Tag used for the targeting preset that discovered this interactable during Grasp scanning */
	UPROPERTY()
	FGameplayTag ScanTag;

	/** The actor being interacted with, e.g. a door or chest */
	UPROPERTY()
	TWeakObjectPtr<const UPrimitiveComponent> Graspable;

	/**
	 * Normalized Distance between Avatar and Graspable location on a 0-1 scale
	 * Normalized between 0 and the max scan range
	 * Abilities are granted when we're closer and removed when we're further away
	 */
	UPROPERTY()
	float NormalizedScanDistance;

	bool operator==(const FGraspScanResult& Other) const
	{
		return Graspable == Other.Graspable;
	}

	bool operator!=(const FGraspScanResult& Other) const
	{
		return !(*this == Other);
	}
};
DECLARE_DELEGATE_TwoParams(FOnGraspTargetsReady, UGraspComponent* GraspComponent, const TArray<FGraspScanResult>& Results);

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