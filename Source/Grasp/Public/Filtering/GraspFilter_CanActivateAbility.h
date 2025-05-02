// Copyright (c) Jared Taylor

#pragma once

#include "CoreMinimal.h"
#include "GraspTypes.h"
#include "Tasks/TargetingFilterTask_BasicFilterTemplate.h"
#include "GraspFilter_CanActivateAbility.generated.h"

/**
 * Filter targets by whether they have a grasp ability that can be activated or not
 * (i.e. calls CanActivateAbility() on the ability itself, and pre-requisite checks)
 * Designed to be executed by Vigil, or any other focus system that uses Targeting System
 */
UCLASS(Blueprintable, DisplayName="Grasp Filter (Can Activate Grasp Ability)")
class GRASP_API UGraspFilter_CanActivateAbility : public UTargetingFilterTask_BasicFilterTemplate
{
	GENERATED_BODY()

public:
	/** How Grasp abilities retrieve their GraspableComponent -- Determine what checks are done from the ability */
	UPROPERTY(EditAnywhere, Category="Grasp Filter")
	EGraspAbilityComponentSource Source = EGraspAbilityComponentSource::EventData;

public:
	UGraspFilter_CanActivateAbility(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Called against every target data to determine if the target should be filtered out */
	virtual bool ShouldFilterTarget(const FTargetingRequestHandle& TargetingHandle, const FTargetingDefaultResultData& TargetData) const override;
};
