// Copyright (c) Jared Taylor


#include "Targeting/GraspTargetingStatics.h"

#include "Targeting/GraspTargetingTypes.h"
#include "Types/TargetingSystemTypes.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GraspTargetingStatics)

FVector UGraspTargetingStatics::GetSourceLocation(const FTargetingRequestHandle& TargetingHandle,
	EGraspTargetLocationSource LocationSource)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GraspTargetingStatics::GetSourceLocation);
	
	if (const FTargetingSourceContext* SourceContext = FTargetingSourceContext::Find(TargetingHandle))
	{
		if (SourceContext->SourceActor)
		{
			switch (LocationSource)
			{
			case EGraspTargetLocationSource::Actor: return SourceContext->SourceActor->GetActorLocation();
			case EGraspTargetLocationSource::ViewLocation:
				{
					if (const APawn* Pawn = Cast<APawn>(SourceContext->SourceActor))
					{
						return Pawn->GetPawnViewLocation();
					}
				}
			case EGraspTargetLocationSource::Camera:
				{
					if (const APlayerController* PC = Cast<APlayerController>(SourceContext->SourceActor->GetOwner()))
					{
						if (PC->PlayerCameraManager)
						{
							return PC->PlayerCameraManager->GetCameraLocation();
						}
					}
				}
				break;
			}
		}
	}
	return FVector::ZeroVector;
}

FVector UGraspTargetingStatics::GetSourceOffset(const FTargetingRequestHandle& TargetingHandle,
	EGraspTargetLocationSource LocationSource, FVector DefaultSourceLocationOffset, bool bUseRelativeLocationOffset)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GraspTargetingStatics::GetSourceOffset);
	
	if (!bUseRelativeLocationOffset)
	{
		return DefaultSourceLocationOffset;
	}

	if (DefaultSourceLocationOffset.IsZero())
	{
		return FVector::ZeroVector;
	}
	
	if (const FTargetingSourceContext* SourceContext = FTargetingSourceContext::Find(TargetingHandle))
	{
		if (SourceContext->SourceActor)
		{
			switch (LocationSource)
			{
			case EGraspTargetLocationSource::Actor: return SourceContext->SourceActor->GetActorRotation().RotateVector(DefaultSourceLocationOffset);
			case EGraspTargetLocationSource::ViewLocation:
				{
					if (const APawn* Pawn = Cast<APawn>(SourceContext->SourceActor))
					{
						return Pawn->GetViewRotation().RotateVector(DefaultSourceLocationOffset);
					}
				}
			case EGraspTargetLocationSource::Camera:
				{
					if (const APlayerController* PC = Cast<APlayerController>(SourceContext->SourceActor->GetOwner()))
					{
						if (PC->PlayerCameraManager)
						{
							return PC->PlayerCameraManager->GetCameraRotation().RotateVector(DefaultSourceLocationOffset);
						}
					}
				}
				break;
			}
		}
	}
	return FVector::ZeroVector;
}

FQuat UGraspTargetingStatics::GetSourceRotation(const FTargetingRequestHandle& TargetingHandle,
	EGraspTargetRotationSource RotationSource, TArray<EGraspTargetRotationSource> FallbackRotationSources,
	int32 FallbackIndex)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GraspTargetingStatics::GetSourceRotation);
	
	if (const FTargetingSourceContext* SourceContext = FTargetingSourceContext::Find(TargetingHandle))
	{
		if (SourceContext->SourceActor)
		{
			switch (RotationSource)
			{
			case EGraspTargetRotationSource::Actor: return SourceContext->SourceActor->GetActorQuat();
			case EGraspTargetRotationSource::ControlRotation:
				{
					if (const APawn* Pawn = Cast<APawn>(SourceContext->SourceActor))
					{
						return Pawn->GetControlRotation().Quaternion();
					}
				}
			case EGraspTargetRotationSource::ViewRotation:
				{
					if (const APawn* Pawn = Cast<APawn>(SourceContext->SourceActor))
					{
						return Pawn->GetViewRotation().Quaternion();
					}
				}
				break;
			case EGraspTargetRotationSource::Velocity:
				if (const APawn* Pawn = Cast<APawn>(SourceContext->SourceActor))
				{
					if (!Pawn->GetVelocity().IsNearlyZero())
					{
						return Pawn->GetVelocity().ToOrientationQuat();
					}
				}
				break;
			case EGraspTargetRotationSource::Acceleration:
				{
					if (const ACharacter* Character = Cast<ACharacter>(SourceContext->SourceActor))
					{
						if (Character->GetCharacterMovement())
						{
							if (!Character->GetCharacterMovement()->GetCurrentAcceleration().IsNearlyZero())
							{
								return Character->GetCharacterMovement()->GetCurrentAcceleration().ToOrientationQuat();
							}
						}
					}
					break;
				}
			}
		}
	}

	// Fallback to other sources
	if (FallbackRotationSources.IsValidIndex(FallbackIndex))
	{
		RotationSource = FallbackRotationSources[FallbackIndex];
		FallbackIndex++;
		return GetSourceRotation(TargetingHandle, RotationSource, FallbackRotationSources, FallbackIndex);
	}
	
	return FQuat::Identity;
}

void UGraspTargetingStatics::InitCollisionParams(const FTargetingRequestHandle& TargetingHandle,
	FCollisionQueryParams& OutParams, bool bIgnoreSourceActor, bool bIgnoreInstigatorActor, bool bTraceComplex)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GraspTargetingStatics::InitCollisionParams);
	
	if (const FTargetingSourceContext* SourceContext = FTargetingSourceContext::Find(TargetingHandle))
	{
		if (bIgnoreSourceActor && SourceContext->SourceActor)
		{
			OutParams.AddIgnoredActor(SourceContext->SourceActor);
		}

		if (bIgnoreInstigatorActor && SourceContext->InstigatorActor)
		{
			OutParams.AddIgnoredActor(SourceContext->InstigatorActor);
		}
	}

	OutParams.bTraceComplex = bTraceComplex;
	
	// This isn't exported, we can't get to it...
	
	// // If we have a data store to override the collision query params from, add that to our collision params
	// if (const FCollisionQueryTaskData* FoundOverride = UE::TargetingSystem::TTargetingDataStore<FCollisionQueryTaskData>::Find(TargetingHandle))
	// {
	// 	OutParams.AddIgnoredActors(FoundOverride->IgnoredActors);
	// }

	// Grasp is its own thing, lets not use the CVar
	
	// static const auto CVarComplexTracingAOE = IConsoleManager::Get().FindConsoleVariable(TEXT("ts.AOE.EnableComplexTracingAOE"));
	// const bool bComplexTracingAOE = CVarComplexTracingAOE ? CVarComplexTracingAOE->GetBool() : true;
}