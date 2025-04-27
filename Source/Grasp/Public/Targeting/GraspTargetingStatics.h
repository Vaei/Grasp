// Copyright (c) Jared Taylor

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GraspTargetingTypes.h"
#include "Engine/EngineTypes.h"
#include "CollisionQueryParams.h" 
#include "GraspTargetingStatics.generated.h"

struct FGraspFocusResult;
struct FTargetingRequestHandle;

/**
 * Helper functions for Grasp Targeting
 */
UCLASS()
class GRASP_API UGraspTargetingStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Native Event to get the source location for the AOE */
	UFUNCTION(BlueprintCallable, Category=Grasp)
	static FVector GetSourceLocation(const FTargetingRequestHandle& TargetingHandle, EGraspTargetLocationSource LocationSource);

	/** Native Event to get a source location offset for the AOE */
	UFUNCTION(BlueprintCallable, Category=Grasp)
	static FVector GetSourceOffset(const FTargetingRequestHandle& TargetingHandle, EGraspTargetLocationSource LocationSource,
		FVector DefaultSourceLocationOffset = FVector::ZeroVector, bool bUseRelativeLocationOffset = true);

	/** Native event to get the source rotation for the AOE  */
	UFUNCTION(BlueprintCallable, Category=Grasp)
	static FQuat GetSourceRotation(const FTargetingRequestHandle& TargetingHandle,
		EGraspTargetRotationSource RotationSource, TArray<EGraspTargetRotationSource> FallbackRotationSources,
		int32 FallbackIndex = 0);

	/** Setup CollisionQueryParams for the AOE */
	static void InitCollisionParams(const FTargetingRequestHandle& TargetingHandle, FCollisionQueryParams& OutParams,
		bool bIgnoreSourceActor = true, bool bIgnoreInstigatorActor = false, bool bTraceComplex = false);
};
