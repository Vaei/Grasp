// Copyright (c) Jared Taylor


#include "GraspData.h"
#include "Abilities/GameplayAbility.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(GraspData)


TSubclassOf<UGameplayAbility> UGraspData::GetGraspAbility_Implementation() const
{
	return GraspAbility;
}

bool UGraspData::ShouldUseAIParams(const AActor* Interactor) const
{
	if (!bAIUseSeparateParams || !Interactor)
	{
		return false;
	}

	const APawn* Pawn = Cast<APawn>(Interactor);
	if (!Pawn)
	{
		if (const AController* Controller = Cast<AController>(Interactor))
		{
			Pawn = Controller->GetPawn();
		}
	}
	return Pawn && Pawn->IsBotControlled();
}

#if WITH_EDITOR

void UGraspData::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.GetPropertyName().IsEqual(GET_MEMBER_NAME_CHECKED(ThisClass, MaxGraspDistance)) ||
		PropertyChangedEvent.GetPropertyName().IsEqual(GET_MEMBER_NAME_CHECKED(ThisClass, MaxHighlightDistance)))
	{
		if (FMath::IsNearlyZero(MaxHighlightDistance))
		{
			MaxHighlightDistance = 0.f;
		}
		else
		{
			// Clamp the highlight distance to be greater or equal than the grasp distance
			MaxHighlightDistance = FMath::Max(MaxHighlightDistance, MaxGraspDistance);
		}
	}
}

EDataValidationResult UGraspData::IsDataValid(class FDataValidationContext& Context) const
{
	// We can't interact if angle is 0
	if (FMath::IsNearlyZero(MaxGraspAngle))
	{
		Context.AddError(NSLOCTEXT("GraspData", "InvalidGraspData", "MaxGraspAngle should be greater than 0"));
		return EDataValidationResult::Invalid;
	}
	
	// MaxHighlightDistance should be greater than MaxGraspDistance, unless MaxHighlightDistance is 0
	if (MaxHighlightDistance > 0.f && MaxHighlightDistance < MaxGraspDistance)
	{
		Context.AddError(FText::Format(NSLOCTEXT("GraspData", "InvalidGraspData", "MaxHighlightDistance ({0}) should either be 0 (disabled) or should be greater than MaxGraspDistance ({1})"),
			FText::AsNumber(MaxHighlightDistance), FText::AsNumber(MaxGraspDistance)));
		return EDataValidationResult::Invalid;
	}
	
	return Super::IsDataValid(Context);
}
#endif
