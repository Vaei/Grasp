// Copyright (c) Jared Taylor

#pragma once

#include "CoreMinimal.h"
#include "Tasks/TargetingFilterTask_BasicFilterTemplate.h"
#include "GraspFilter_Graspable.generated.h"

/**
 * Filter targets by IGraspComponent and IGraspableOwner
 * Mandatory for Grasp interaction -- no other implement checks are performed
 */
UCLASS(Blueprintable, DisplayName="Grasp Filter (Graspable Interface)")
class GRASP_API UGraspFilter_Graspable : public UTargetingFilterTask_BasicFilterTemplate
{
	GENERATED_BODY()

public:
	UGraspFilter_Graspable(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Called against every target data to determine if the target should be filtered out */
	virtual bool ShouldFilterTarget(const FTargetingRequestHandle& TargetingHandle, const FTargetingDefaultResultData& TargetData) const override;
};
