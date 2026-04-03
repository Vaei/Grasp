// Copyright (c) Jared Taylor

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Templates/SubclassOf.h"
#include "GraspRequestTypes.generated.h"

class UGraspData;

/**
 * Filter criteria for searching graspable components.
 */
USTRUCT(BlueprintType)
struct GRASP_API FGraspRequestFilter
{
	GENERATED_BODY()

	/** Only return graspables whose GraspData InputTag matches this query. Leave empty to match all. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Grasp)
	FGameplayTagQuery TagRequirements;

	/** If set, only return graspables with this specific GraspData class (or subclass). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Grasp)
	TSubclassOf<UGraspData> GraspDataClass;

	/** If true, include graspables that report IsGraspableDead() == true. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Grasp)
	bool bIncludeDead = false;

	/** If true, include graspables with no GraspAbility set. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Grasp)
	bool bIncludeWithoutAbility = false;
};

/**
 * Spatial search request for graspable components.
 */
USTRUCT(BlueprintType)
struct GRASP_API FGraspRequest
{
	GENERATED_BODY()

	FGraspRequest() = default;

	/** Box-shaped search volume. Used when bUseSphere is false. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Grasp)
	FBox QueryBox = FBox(ForceInitToZero);

	/** Center of sphere search. Used when bUseSphere is true. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Grasp)
	FVector QuerySphereCenter = FVector::ZeroVector;

	/** Radius of sphere search. Used when bUseSphere is true. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Grasp, meta = (ClampMin = "0"))
	float QuerySphereRadius = 0.f;

	/** If true, use QuerySphereCenter + QuerySphereRadius instead of QueryBox. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Grasp)
	bool bUseSphere = false;

	/** Filter criteria. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Grasp)
	FGraspRequestFilter Filter;
};

/**
 * Single result from a graspable search.
 */
USTRUCT(BlueprintType)
struct GRASP_API FGraspRequestResult
{
	GENERATED_BODY()

	FGraspRequestResult() = default;

	FGraspRequestResult(UPrimitiveComponent* InGraspableComponent, int32 InGraspDataIndex, float InDistance = 0.f)
		: GraspableComponent(InGraspableComponent)
		, GraspDataIndex(InGraspDataIndex)
		, Distance(InDistance)
	{}

	bool IsValid() const { return GraspableComponent.IsValid() && GraspDataIndex >= 0; }

	/** The graspable component that was found. Implements IGraspableComponent. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = Grasp)
	TWeakObjectPtr<UPrimitiveComponent> GraspableComponent;

	/** Index into the component's GraspData entries. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = Grasp)
	int32 GraspDataIndex = INDEX_NONE;

	/** Distance from the query origin to this graspable. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = Grasp)
	float Distance = 0.f;

	bool operator==(const FGraspRequestResult& Other) const
	{
		return GraspableComponent == Other.GraspableComponent && GraspDataIndex == Other.GraspDataIndex;
	}

	bool operator!=(const FGraspRequestResult& Other) const
	{
		return !(*this == Other);
	}
};
