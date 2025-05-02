// Copyright (c) Jared Taylor


#include "Filtering/GraspFilter_IsWithinGraspableData.h"

#include "GraspComponent.h"
#include "GraspStatics.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GraspFilter_IsWithinGraspableData)


UGraspFilter_IsWithinGraspableData::UGraspFilter_IsWithinGraspableData(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{}

bool UGraspFilter_IsWithinGraspableData::ShouldFilterTarget(const FTargetingRequestHandle& TargetingHandle,
	const FTargetingDefaultResultData& TargetData) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GraspFilter_IsWithinGraspableData::ShouldFilterTarget);

	// Find the source actor
	const FTargetingSourceContext* SourceContext = FTargetingSourceContext::Find(TargetingHandle);
	if (!SourceContext || !IsValid(SourceContext->SourceActor))
	{
		return true;
	}

	// Get the grasp component from the source actor
	const TObjectPtr<AActor> SourceActor = SourceContext->SourceActor;
	const UPrimitiveComponent* TargetComponent = TargetData.HitResult.GetComponent();

	// Query if we can interact with the target based on angle and distance
	float NormalizedAngleDiff, NormalizedDistance, NormalizedHighlightDistance = 0.f;
	const EGraspQueryResult Result = UGraspStatics::CanInteractWith(SourceActor, TargetComponent,
		NormalizedAngleDiff, NormalizedDistance, NormalizedHighlightDistance);

	// Return the result based on the threshold
	bool bCanInteract;
	switch (Result)
	{
	case EGraspQueryResult::Highlight:
		bCanInteract = Threshold == EGraspQueryResult::Highlight;
		break;
	case EGraspQueryResult::Interact:
		bCanInteract = true;
		break;
	default:
		return true;
	}
	
	return !bCanInteract;
}
