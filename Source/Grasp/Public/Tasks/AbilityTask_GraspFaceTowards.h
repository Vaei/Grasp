// Copyright (c) Jared Taylor

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "AbilityTask_GraspFaceTowards.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FApplyFaceTowardsDelegate);

class UCharacterMovementComponent;

/**
 * Face towards a target over time
 */
UCLASS()
class GRASP_API UAbilityTask_GraspFaceTowards : public UAbilityTask
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FApplyFaceTowardsDelegate OnFinish;
	
	UAbilityTask_GraspFaceTowards(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;

	/**
	 * Apply force to character's movement to rotate towards a target over time
	 * Bind to OnFinish then call SharedInitAndApply() after creating this task -- otherwise the task will not run
	 * 
	 * @param OwningAbility Ability that owns this task
	 * @param TargetComponent Optional Component to face towards
	 * @param TargetActor Optional Actor to face towards, used if TargetComponent is null
	 * @param WorldDirection Optional Direction to face towards, used if TargetComponent and TargetActor are null
	 * @param WorldLocation Optional Location to face towards, used if TargetComponent and TargetActor are null and WorldDirection is zero
	 * @param Duration How long to face towards the target
	 * @param bFace2D If true, normalize the face direction to 2D space
	 * @param bEnableGravity If false, gravity will be ignored while the task is active
	 * @param bStopWhenAbilityEnds If true, the impulse will stop when the ability ends
	 */
	static UAbilityTask_GraspFaceTowards* FaceTowards
	(
		UGameplayAbility* OwningAbility,
		const TObjectPtr<const USceneComponent>& TargetComponent = nullptr,
		const TObjectPtr<const AActor>& TargetActor = nullptr,
		const FVector WorldDirection = FVector::ZeroVector,
		const FVector WorldLocation = FVector::ZeroVector,
		float Duration = 0.2f,
		bool bFace2D = true,
		bool bEnableGravity = true,
		bool bStopWhenAbilityEnds = false
	);

	/**
	 * Apply force to character's movement to rotate towards a target over time
	 * @param OwningAbility Ability that owns this task
	 * @param TargetComponent Optional Component to face towards
	 * @param TargetActor Optional Actor to face towards, used if TargetComponent is null
	 * @param WorldDirection Optional Direction to face towards, used if TargetComponent and TargetActor are null
	 * @param WorldLocation Optional Location to face towards, used if TargetComponent and TargetActor are null and WorldDirection is zero
	 * @param Duration How long to face towards the target
	 * @param bFace2D If true, normalize the face direction to 2D space
	 * @param bEnableGravity If false, gravity will be ignored while the task is active
	 * @param bStopWhenAbilityEnds If true, the impulse will stop when the ability ends
	 */
	UFUNCTION(BlueprintCallable, Category="Ability|Tasks", meta=(DisplayName="Face Towards (Grasp)", Keywords="rotate,grasp", HidePin="OwningAbility", DefaultToSelf="OwningAbility", BlueprintInternalUseOnly="TRUE"))
	static UAbilityTask_GraspFaceTowards* K2_FaceTowards
	(
		UGameplayAbility* OwningAbility,
		const USceneComponent* TargetComponent = nullptr,
		const AActor* TargetActor = nullptr,
		const FVector WorldDirection = FVector::ZeroVector,
		const FVector WorldLocation = FVector::ZeroVector,
		float Duration = 0.2f,
		bool bFace2D = true,
		bool bEnableGravity = true,
		bool bStopWhenAbilityEnds = false
	);

	virtual void InitSimulatedTask(UGameplayTasksComponent& InGameplayTasksComponent) override;
	virtual void SharedInitAndApply();

	virtual void TickTask(float DeltaTime) override;
	bool HasTimedOut() const;

	virtual void PreDestroyFromReplication() override;
	virtual void OnDestroy(bool bInOwnerFinished) override;
	
protected:
	UPROPERTY(Replicated)
	TObjectPtr<const AActor> TargetActor;

	UPROPERTY(Replicated)
	TObjectPtr<const USceneComponent> TargetComponent;
	
	UPROPERTY(Replicated)
	FVector WorldLocation;
	
	UPROPERTY(Replicated)
	FVector WorldDirection;
	
	UPROPERTY(Replicated)
	bool bFace2D;
	
	UPROPERTY(Replicated)
	float Duration;

	UPROPERTY(Replicated)
	bool bEnableGravity;
	
	UPROPERTY(Replicated)
	bool bStopWhenAbilityEnds;
	
	UPROPERTY()
	TObjectPtr<UCharacterMovementComponent> MovementComponent; 

	uint16 RootMotionSourceID;

	bool bIsFinished;
};