// Copyright (c) Jared Taylor


#include "Tasks/AbilityTask_GraspFaceTowards.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemLog.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/RootMotionSource.h"
#include "Tasks/RootMotionSource_GraspFaceTowards.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AbilityTask_GraspFaceTowards)


UAbilityTask_GraspFaceTowards::UAbilityTask_GraspFaceTowards(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bTickingTask = true;
	bSimulatedTask = true;
	Priority = 4;
	RootMotionSourceID = (uint16)ERootMotionSourceID::Invalid;

	TargetActor = nullptr;
	TargetComponent = nullptr;
	WorldLocation = FVector::ZeroVector;
	WorldDirection = FVector::ZeroVector;
	Duration = 0.2f;
	bFace2D = true;
	bEnableGravity = false;
	bStopWhenAbilityEnds = false;

	MovementComponent = nullptr;
	bIsFinished = false;
}

UAbilityTask_GraspFaceTowards* UAbilityTask_GraspFaceTowards::FaceTowards(UGameplayAbility* OwningAbility,
	const TObjectPtr<const USceneComponent>& TargetComponent, const TObjectPtr<const AActor>& TargetActor,
	const FVector WorldDirection, const FVector WorldLocation, float Duration, bool bFace2D, bool bEnableGravity,
	bool bStopWhenAbilityEnds)
{
	auto* MyTask = NewAbilityTask<UAbilityTask_GraspFaceTowards>(OwningAbility, StaticClass()->GetFName());

	MyTask->TargetActor = TargetActor;
	MyTask->TargetComponent = TargetComponent;
	MyTask->WorldLocation = WorldLocation;
	MyTask->WorldDirection = WorldDirection.GetSafeNormal();
	MyTask->Duration = Duration;
	MyTask->bFace2D = bFace2D;
	MyTask->bEnableGravity = bEnableGravity;
	MyTask->bStopWhenAbilityEnds = bStopWhenAbilityEnds;
	
	// Bind to OnFinished then call SharedInitAndApply() after creating this task
	// Otherwise the task will not run
	
	return MyTask;
}

UAbilityTask_GraspFaceTowards* UAbilityTask_GraspFaceTowards::K2_FaceTowards(UGameplayAbility* OwningAbility,
	const USceneComponent* TargetComponent, const AActor* TargetActor,
	const FVector WorldDirection, const FVector WorldLocation, float Duration, bool bFace2D, bool bEnableGravity,
	bool bStopWhenAbilityEnds)
{
	UAbilityTask_GraspFaceTowards* MyTask = FaceTowards(OwningAbility, TargetComponent, TargetActor, WorldDirection, WorldLocation, Duration,
		bFace2D, bEnableGravity, bStopWhenAbilityEnds);

	MyTask->SharedInitAndApply();
	return MyTask;
}

void UAbilityTask_GraspFaceTowards::InitSimulatedTask(UGameplayTasksComponent& InGameplayTasksComponent)
{
	Super::InitSimulatedTask(InGameplayTasksComponent);
	SharedInitAndApply();
}

void UAbilityTask_GraspFaceTowards::SharedInitAndApply()
{
	const UAbilitySystemComponent* ASC = AbilitySystemComponent.Get();
	if (ASC && ASC->AbilityActorInfo->MovementComponent.IsValid())
	{
		const AActor* AvatarActor = ASC->AbilityActorInfo->AvatarActor.Get();
		MovementComponent = Cast<UCharacterMovementComponent>(ASC->AbilityActorInfo->MovementComponent.Get());

		if (MovementComponent)
		{
			const TSharedPtr<FRootMotionSource_GraspFaceTowards> FaceTowards = MakeShared<FRootMotionSource_GraspFaceTowards>();
			FaceTowards->InstanceName = TEXT("FaceTowards");
			FaceTowards->Priority = Priority;
			FaceTowards->Duration = FMath::Max(0.001f, Duration);
			
			const FVector AvatarLocation = AvatarActor->GetActorLocation();
			FaceTowards->StartDirection = AvatarActor->GetActorForwardVector();

			// Resolve the target direction
			if (TargetComponent != nullptr)
			{
				FaceTowards->TargetDirection = (TargetComponent->GetComponentLocation() - AvatarLocation);
			}
			else if (TargetActor != nullptr)
			{
				FaceTowards->TargetDirection = (TargetActor->GetActorLocation() - AvatarLocation);
			}
			else if (!WorldDirection.IsZero())
			{
				FaceTowards->TargetDirection = WorldDirection;
			}
			else
			{
				FaceTowards->TargetDirection = (WorldLocation - AvatarLocation);
			}

			// Normalize the direction
			FaceTowards->TargetDirection = bFace2D ? FaceTowards->TargetDirection.GetSafeNormal2D() :
				FaceTowards->TargetDirection.GetSafeNormal();

			// Gravity
			if (bEnableGravity)
			{
				FaceTowards->Settings.SetFlag(ERootMotionSourceSettingsFlags::IgnoreZAccumulate);
			}

			// Apply the source
			RootMotionSourceID = MovementComponent->ApplyRootMotionSource(FaceTowards);
		}
	}
	else
	{
		ABILITY_LOG(Warning, TEXT("UHonorAbilityTask_FaceTowards called in Ability %s with null MovementComponent; Task Instance Name %s."), 
			Ability ? *Ability->GetName() : TEXT("NULL"), 
			*InstanceName.ToString());
		
		EndTask();
	}
}

void UAbilityTask_GraspFaceTowards::TickTask(float DeltaTime)
{
	if (bIsFinished)
	{
		return;
	}

	AActor* MyActor = GetAvatarActor();
	if (MyActor)
	{
		const bool bTimedOut = HasTimedOut();
		const bool bIsInfiniteDuration = Duration < 0.f;

		if (!bIsInfiniteDuration && bTimedOut)
		{
			// Task has finished
			bIsFinished = true;
			if (!bIsSimulating)
			{
				MyActor->ForceNetUpdate();
				if (ShouldBroadcastAbilityTaskDelegates())
				{
					OnFinish.Broadcast();
				}
				EndTask();
			}
		}
	}
	else
	{
		bIsFinished = true;
		EndTask();
	}
}

bool UAbilityTask_GraspFaceTowards::HasTimedOut() const
{
	const TSharedPtr<FRootMotionSource> RMS = MovementComponent ?
		MovementComponent->GetRootMotionSourceByID(RootMotionSourceID) : nullptr;
	
	if (!RMS.IsValid())
	{
		return true;
	}

	return RMS->Status.HasFlag(ERootMotionSourceStatusFlags::Finished);
}

void UAbilityTask_GraspFaceTowards::PreDestroyFromReplication()
{
	bIsFinished = true;
	EndTask();
}

void UAbilityTask_GraspFaceTowards::OnDestroy(bool bInOwnerFinished)
{
	if (MovementComponent)
	{
		if (!bIsFinished && ShouldBroadcastAbilityTaskDelegates())
		{
			bIsFinished = true;
			OnFinish.Broadcast();
		}
	
		MovementComponent->RemoveRootMotionSourceByID(RootMotionSourceID);
	}
	
	Super::OnDestroy(bInOwnerFinished);
}
