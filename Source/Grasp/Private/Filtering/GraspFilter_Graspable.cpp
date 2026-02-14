// Copyright (c) Jared Taylor


#include "Filtering/GraspFilter_Graspable.h"

#include "GraspableComponent.h"
#include "GraspableOwner.h"
#include "GraspData.h"
#include "Abilities/GameplayAbility.h"
#include "Components/PrimitiveComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GraspFilter_Graspable)


UGraspFilter_Graspable::UGraspFilter_Graspable(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{}

bool UGraspFilter_Graspable::ShouldFilterTarget(const FTargetingRequestHandle& TargetingHandle,
	const FTargetingDefaultResultData& TargetData) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GraspFilter_Graspable::ShouldFilterTarget);

	const AActor* TargetActor = TargetData.HitResult.GetActor();

	// Check if the target actor is valid
	if (!IsValid(TargetActor))
	{
		return true;
	}
	
	// Check if pending kill or torn off
	if (TargetActor->IsPendingKillPending() || TargetActor->GetTearOff())
	{
		return true;
	}

	// Retrieve the target component and interface
	const UPrimitiveComponent* TargetComponent = TargetData.HitResult.GetComponent();
	const IGraspableComponent* Graspable = TargetComponent ? Cast<IGraspableComponent>(TargetComponent) : nullptr;
	
	// Check if graspable is valid
	if (!Graspable)
	{
		return true;
	}

	// Check if any GraspData entry has a valid ability
	bool bHasValidEntry = false;
	for (int32 i = 0; i < Graspable->GetNumGraspData(); i++)
	{
		const UGraspData* GraspData = Graspable->GetGraspData(i);
		if (GraspData && GraspData->GetGraspAbility())
		{
			bHasValidEntry = true;
			break;
		}
	}
	if (!bHasValidEntry)
	{
		return true;
	}

	// Check if the target is dead
	if (Graspable->IsGraspableDead())
	{
		return true;
	}

	// If implementing owner interface, check again
	if (TargetActor->Implements<UGraspableOwner>() && IGraspableOwner::Execute_IsGraspableDead(TargetActor))
	{
		return true;
	}
	
	return false;
}
