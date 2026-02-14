// Copyright (c) Jared Taylor


#include "Filtering/GraspFilter_IsWithinGraspableAngle.h"

#include "GraspableComponent.h"
#include "GraspComponent.h"
#include "GraspStatics.h"
#include "Components/PrimitiveComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GraspFilter_IsWithinGraspableAngle)


UGraspFilter_IsWithinGraspableAngle::UGraspFilter_IsWithinGraspableAngle(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{}

bool UGraspFilter_IsWithinGraspableAngle::ShouldFilterTarget(const FTargetingRequestHandle& TargetingHandle,
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

	// Check if ANY GraspData entry passes the angle filter
	for (int32 i = 0; i < Graspable->GetNumGraspData(); i++)
	{
		float NormalizedAngle = 0.f;
		if (UGraspStatics::CanInteractWithAngle(SourceActor, TargetComponent, NormalizedAngle, i))
		{
			return false;
		}
	}

	return true;
}
