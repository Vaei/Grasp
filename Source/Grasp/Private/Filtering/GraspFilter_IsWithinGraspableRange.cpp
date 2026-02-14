// Copyright (c) Jared Taylor


#include "Filtering/GraspFilter_IsWithinGraspableRange.h"

#include "GraspableComponent.h"
#include "GraspComponent.h"
#include "GraspStatics.h"
#include "Components/PrimitiveComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GraspFilter_IsWithinGraspableRange)


UGraspFilter_IsWithinGraspableRange::UGraspFilter_IsWithinGraspableRange(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{}

bool UGraspFilter_IsWithinGraspableRange::ShouldFilterTarget(const FTargetingRequestHandle& TargetingHandle,
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

	const IGraspableComponent* Graspable = TargetComponent ? Cast<IGraspableComponent>(TargetComponent) : nullptr;
	if (!Graspable)
	{
		return true;
	}

	// Check if ANY GraspData entry passes the filter
	bool bAnyPassesFilter = false;
	for (int32 i = 0; i < Graspable->GetNumGraspData(); i++)
	{
		float NormalizedDistance, NormalizedHighlightDistance = 0.f;
		const EGraspQueryResult Result = UGraspStatics::CanInteractWithRange(SourceActor, TargetComponent,
			NormalizedDistance, NormalizedHighlightDistance, i);

		const bool bPasses = (Result == EGraspQueryResult::Interact) ||
			(Result == EGraspQueryResult::Highlight && Threshold == EGraspQueryResult::Highlight);
		if (bPasses)
		{
			bAnyPassesFilter = true;
			break;
		}
	}

	return !bAnyPassesFilter;
}
