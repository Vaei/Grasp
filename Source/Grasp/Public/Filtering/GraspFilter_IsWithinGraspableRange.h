// Copyright (c) Jared Taylor

#pragma once

#include "CoreMinimal.h"
#include "GraspTypes.h"
#include "Tasks/TargetingFilterTask_BasicFilterTemplate.h"
#include "GraspFilter_IsWithinGraspableRange.generated.h"


/**
 * Filter targets by whether they are within the parameters defined in UGraspData such as the angle and distance
 */
UCLASS(Blueprintable, DisplayName="Grasp Filter (Graspable Range)")
class GRASP_API UGraspFilter_IsWithinGraspableRange : public UTargetingFilterTask_BasicFilterTemplate
{
	GENERATED_BODY()

public:
	/**
	 * What result we must pass to not be filtered out
	 * This filter can be used to find targets that can be interacted with only, or targets that can be highlighted
	 */
	UPROPERTY(EditAnywhere, Category="Grasp Filter", meta=(InvalidEnumValues="None"))
	EGraspQueryResult Threshold = EGraspQueryResult::Interact;

	/** Which up direction the check is measured against */
	UPROPERTY(EditAnywhere, Category="Grasp Filter")
	EGraspUpMode UpMode = EGraspUpMode::WorldUp;

	/** The up vector used when UpMode is CustomUp */
	UPROPERTY(EditAnywhere, Category="Grasp Filter", meta=(EditCondition="UpMode==EGraspUpMode::CustomUp", EditConditionHides))
	FVector CustomUp = FVector::UpVector;

public:
	UGraspFilter_IsWithinGraspableRange(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Called against every target data to determine if the target should be filtered out */
	virtual bool ShouldFilterTarget(const FTargetingRequestHandle& TargetingHandle, const FTargetingDefaultResultData& TargetData) const override;

protected:
	/** Up basis for this query; defaults to the configured UpMode/CustomUp. Override to resolve per-interactor. */
	virtual EGraspUpMode GetUpParams(const AActor* SourceActor, FVector& OutCustomUp) const
	{
		OutCustomUp = CustomUp;
		return UpMode;
	}
};
