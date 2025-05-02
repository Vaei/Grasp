// Copyright (c) Jared Taylor

#pragma once

#include "CoreMinimal.h"
#include "GraspTypes.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "GraspScanTask.generated.h"

struct FTargetingRequestHandle;
class UGraspComponent;

/**
 * Grasp's passive perpetual task that scans for interactables nearing interaction range to grant their abilities to the owner
 * Should only run on Authority
 */
UCLASS(Config=Game)
class GRASP_API UGraspScanTask : public UAbilityTask
{
	GENERATED_BODY()

public:
	FTimerHandle GraspWaitTimer;

protected:
	UPROPERTY()
	TWeakObjectPtr<UGraspComponent> GC;

	TOptional<FString> WaitReason;
	TOptional<FString> VeryVerboseWaitReason;
	
public:
	UGraspScanTask(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	
	/**
	 * Grasp's passive perpetual task that scans for interact targets to grant an ability to, prior to interaction
	 * @param OwningAbility The ability that owns this task
	 * @param ErrorWaitDelay Delay before we attempt any requests after encountering an error
	 */
	UFUNCTION(BlueprintCallable, Category="Ability|Tasks", meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE", DisplayName="Grasp Scan"))
	static UGraspScanTask* GraspScan(UGameplayAbility* OwningAbility, float ErrorWaitDelay = 0.5f);

	virtual void Activate() override;

	/**
	 * Wait for a bit before trying to request a Grasp again
	 * @param Delay How long to wait before trying again
	 * @param Reason Optional reason for waiting
	 * @param VeryVerboseReason Optional reason for waiting, only used in debug builds
	 */
	void WaitForGrasp(float Delay, const TOptional<FString>& Reason = {}, const TOptional<FString>& VeryVerboseReason = {});

	/** This is the main looping function, that looks for GraspableComponent */
	void RequestGrasp();

	/** Callback for when a GraspableComponent is found */
	void OnGraspComplete(FTargetingRequestHandle TargetingHandle, FGameplayTag ScanTag);

	/** Broadcast from GraspComponent */
	UFUNCTION()
	void OnPauseGrasp(bool bPaused);

	/** Broadcast from GraspComponent after all our tasks were removed, i.e. we never get our callback to continue */
	UFUNCTION()
	void OnRequestGrasp();

	virtual void OnDestroy(bool bInOwnerFinished) override;

protected:
	UPROPERTY()
	float Delay = 0.5f;
	
	ENetMode GetOwnerNetMode() const;
	FString GetRoleString() const;
};
