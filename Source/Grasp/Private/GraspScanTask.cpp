// Copyright (c) Jared Taylor


#include "GraspScanTask.h"

#include "GraspComponent.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Components/PrimitiveComponent.h"
#include "TargetingSystem/TargetingSubsystem.h"
#include "TargetingSystem/TargetingPreset.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h"

#if !UE_BUILD_SHIPPING
#include "Logging/MessageLog.h"
#endif

#include "GraspableComponent.h"
#include "GraspData.h"
#include "GraspDeveloper.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GraspScanTask)

namespace FGraspCVars
{
#if UE_ENABLE_DEBUG_DRAWING
	static int32 GraspScanDebug = 0;
	FAutoConsoleVariableRef CVarGraspScanDebug(
		TEXT("p.Grasp.Scan.Debug"),
		GraspScanDebug,
		TEXT("Optionally draw debug for Grasp Scan Task.\n")
		TEXT("0: Disable, 1: Enable for all, 2: Enable for local player only"),
		ECVF_Default);
#endif

	static bool bLogVeryVerboseScanRequest = true;
	FAutoConsoleVariableRef CVarLogVeryVerboseBroadcastResults(
		TEXT("p.Grasp.Scan.LogVeryVerboseScanRequest"),
		bLogVeryVerboseScanRequest,
		TEXT("Log very verbose requests and broadcast results for Grasp Scan Task"),
		ECVF_Default);
}

#define LOCTEXT_NAMESPACE "GraspScanTask"

UGraspScanTask::UGraspScanTask(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bSimulatedTask = false;
	bTickingTask = false;
}

UGraspScanTask* UGraspScanTask::GraspScan(UGameplayAbility* OwningAbility, float ErrorWaitDelay)
{
#if !UE_BUILD_SHIPPING
	// Ability should always have authority; clients never need to give abilities, which this task is for
	const bool bInvalidAuth = !OwningAbility->K2_HasAuthority();
	const bool bInvalidExec = OwningAbility->GetNetExecutionPolicy() != EGameplayAbilityNetExecutionPolicy::ServerOnly;
	const bool bInvalidSec = OwningAbility->GetNetSecurityPolicy() != EGameplayAbilityNetSecurityPolicy::ServerOnly;
	if (bInvalidAuth || bInvalidExec || bInvalidSec)
	{
		if (!GetDefault<UGraspDeveloper>()->bDisableScanTaskAbilityErrorChecking)
		{
			FMessageLog("PIE").Error(FText::Format(
				LOCTEXT("GraspScanTaskInvalidAbility", "GraspScanTask: Invalid ability: {0} (InvalidAuth: {1}, InvalidExec: {2}, InvalidSec: {3})"),
				FText::FromString(OwningAbility->GetName()),
				FText::FromString(bInvalidAuth ? TEXT("true") : TEXT("false")),
				FText::FromString(bInvalidExec ? TEXT("true") : TEXT("false")),
				FText::FromString(bInvalidSec ? TEXT("true") : TEXT("false"))));
		}
	}
#endif
	
	UGraspScanTask* MyObj = NewAbilityTask<UGraspScanTask>(OwningAbility);
	MyObj->Delay = ErrorWaitDelay;
	return MyObj;
}

void UGraspScanTask::Activate()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GraspScanTask::Activate);

	UE_LOG(LogGrasp, Verbose, TEXT("%s GraspScanTask::Activate"), *GetRoleString());

	SetWaitingOnAvatar();
	RequestGrasp();
}

void UGraspScanTask::WaitForGrasp(float InDelay, const TOptional<FString>& Reason,
	const TOptional<FString>& VeryVerboseReason)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GraspScanTask::WaitForGrasp);

	if (!IsValid(GetWorld()))
	{
		UE_LOG(LogGrasp, Verbose, TEXT("%s GraspScanTask::WaitForGrasp: Invalid world. [SYSTEM END]"),
		       *GetRoleString());
		return;
	}

	WaitReason = Reason;
	VeryVerboseWaitReason = VeryVerboseReason;

	GetWorld()->GetTimerManager().SetTimer(GraspWaitTimer, this, &UGraspScanTask::RequestGrasp, InDelay, false);
}

void UGraspScanTask::RequestGrasp()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GraspScanTask::RequestGrasp);

	// Print the last reason we waited, if set
	if (WaitReason.IsSet())
	{
		UE_LOG(LogGrasp, Verbose,
		       TEXT("%s GraspScanTask::WaitForGrasp: LastWaitReason: %s [SYSTEM RESUME]"),
		       *GetRoleString(), *WaitReason.GetValue());
		WaitReason.Reset();
	}
	if (VeryVerboseWaitReason.IsSet())
	{
		UE_LOG(LogGrasp, VeryVerbose,
		       TEXT("%s GraspScanTask::WaitForGrasp: LastWaitReason: %s [SYSTEM RESUME]"),
		       *GetRoleString(), *VeryVerboseWaitReason.GetValue());
		VeryVerboseWaitReason.Reset();
	}

	// Cache the GraspComponent if required
	if (!GC.IsValid())
	{
		UE_LOG(LogGrasp, Verbose, TEXT("%s GraspScanTask::RequestGrasp: Trying to cache GraspComponent..."),
		       *GetRoleString());

		// Get the owning controller
		const TWeakObjectPtr<AActor>& WeakOwner = Ability->GetCurrentActorInfo()->OwnerActor;
		const AController* Controller = nullptr;
		if (const APawn* Pawn = Cast<APawn>(WeakOwner.Get()))
		{
			UE_LOG(LogGrasp, Verbose, TEXT("%s GraspScanTask::RequestGrasp: Retrieve controller from owner pawn"),
			       *GetRoleString());
			Controller = Pawn->GetController<AController>();
		}
		else if (const APlayerState* PlayerState = Cast<APlayerState>(WeakOwner.Get()))
		{
			UE_LOG(LogGrasp, Verbose,
			       TEXT("%s GraspScanTask::RequestGrasp: Retrieve controller from owner player state"),
			       *GetRoleString());
			Controller = PlayerState->GetOwningController();
		}
		else if (const AController* PC = Cast<AController>(WeakOwner.Get()))
		{
			UE_LOG(LogGrasp, Verbose, TEXT("%s GraspScanTask::RequestGrasp: Owner is a controller"), *GetRoleString());
			Controller = PC;
		}
		else
		{
			UE_LOG(LogGrasp, Error,
			       TEXT(
				       "%s GraspScanTask::RequestGrasp: Could not retrieve controller because owner is not a pawn or player state or controller"
			       ), *GetRoleString());
		}

		// If the controller is not valid, wait for a bit and try again
		if (UNLIKELY(!Controller))
		{
			UE_LOG(LogGrasp, Verbose, TEXT("%s GraspScanTask::RequestGrasp: Invalid controller. [SYSTEM WAIT]"),
			       *GetRoleString());
			WaitForGrasp(Delay, {"Invalid Controller"});
			return;
		}

		// Find the GraspComponent on the controller
		GC = Controller->FindComponentByClass<UGraspComponent>();
		if (!GC.IsValid())
		{
#if !UE_BUILD_SHIPPING
			if (IsInGameThread())
			{
				FMessageLog("PIE").Error(FText::Format(
					LOCTEXT("GraspComponentNotFound", "GraspComponent not found on {0}"),
					FText::FromString(Controller->GetName())));
			}
#endif

			UE_LOG(LogGrasp, Verbose, TEXT("%s GraspScanTask::RequestGrasp: Invalid GraspComponent. [SYSTEM END]"),
			       *GetRoleString());

			// Grasp will not run at all
			return;
		}
		else
		{
			UE_LOG(LogGrasp, Verbose, TEXT("%s GraspScanTask::RequestGrasp: Found and cached GraspComponent: %s"),
			       *GetRoleString(), *GC->GetName());
		}

		// Bind to the pause delegate
		if (!GC->OnPauseGrasp.IsBoundToObject(this))
		{
			UE_LOG(LogGrasp, Verbose, TEXT("%s GraspScanTask::RequestGrasp: Binding to OnPauseGrasp"),
			       *GetRoleString());
			GC->OnPauseGrasp.BindUObject(this, &ThisClass::OnPauseGrasp);
		}

		// Bind to request delegate
		if (!GC->OnRequestGrasp.IsBoundToObject(this))
		{
			UE_LOG(LogGrasp, Verbose, TEXT("%s GraspScanTask::RequestGrasp: Binding to OnRequestGrasp"),
			       *GetRoleString());
			GC->OnRequestGrasp.BindUObject(this, &ThisClass::OnRequestGrasp);
		}
	}

	check(GC.IsValid());

	// Are we on cooldown due to rate throttling?
	const float MaxRate = GC->GetMaxGraspScanRate();
	if (FGraspCVars::bLogVeryVerboseScanRequest)
	{
		UE_LOG(LogGrasp, VeryVerbose, TEXT("%s GraspScanTask::RequestGrasp: MaxRate: %.2f"), *GetRoleString(), MaxRate);
	}
	
	if (MaxRate > 0.f)
	{
		// (Thinking out loud...)
		// If updated at 10.0, rate is 0.1, current time is 10.05
		// TimeSince is 0.05, TimeSince < MaxRate == 0.05 < 0.1 == true
		// TimeLeft = MaxRate - TimeSince == 0.1 - 0.05 == 0.05

		const float TimeSince = GetWorld()->TimeSince(GC->LastGraspScanTime);
		UE_LOG(LogGrasp, VeryVerbose, TEXT("%s GraspScanTask::RequestGrasp: TimeSince: %.2f"), *GetRoleString(),
		       TimeSince);
		if (TimeSince < MaxRate)
		{
			const float TimeLeft = MaxRate - TimeSince;
			UE_LOG(LogGrasp, VeryVerbose, TEXT("%s GraspScanTask::RequestGrasp: TimeLeft: %.2f [SYSTEM WAIT]"),
			       *GetRoleString(), TimeLeft);
			WaitForGrasp(TimeLeft, {}, {"Rate Throttling"});
			return;
		}
		GC->LastGraspScanTime = GetWorld()->GetTimeSeconds();
	}

	// Check if the world and game instance are valid
	if (!GetWorld() || !GetWorld()->GetGameInstance())
	{
		UE_LOG(LogGrasp, Verbose, TEXT("%s GraspScanTask::RequestGrasp: Invalid world or game instance. [SYSTEM WAIT]"),
		       *GetRoleString());
		WaitForGrasp(Delay);
		return;
	}

	// Get the TargetingSubsystem
	UTargetingSubsystem* TargetSubsystem = GetWorld()->GetGameInstance()->GetSubsystem<UTargetingSubsystem>();
	if (!TargetSubsystem)
	{
		UE_LOG(LogGrasp, Verbose, TEXT("%s GraspScanTask::RequestGrasp: Invalid TargetingSubsystem. [SYSTEM WAIT]"),
		       *GetRoleString());
		WaitForGrasp(Delay, {"Invalid TargetingSubsystem"});
		return;
	}

	AActor* TargetingSource = GC->GetTargetingSource();
	if (!TargetingSource)
	{
		UE_LOG(LogGrasp, Verbose, TEXT("%s GraspScanTask::RequestGrasp: Invalid TargetingSource. [SYSTEM WAIT]"),
		       *GetRoleString());
		WaitForGrasp(Delay, {"Invalid TargetingSource"});
		return;
	}

	// Check for changes to the targeting preset update mode
	if (GC->bUpdateTargetingPresetsOnPawnChange != GC->bLastUpdateTargetingPresetsOnPawnChange)
	{
		UE_LOG(LogGrasp, Verbose, TEXT("%s GraspScanTask::RequestGrasp: TargetingPresetUpdateMode changed."),
		       *GetRoleString());
		// Remove or bind the pawn changed binding
		GC->UpdatePawnChangedBinding();
		GC->bLastUpdateTargetingPresetsOnPawnChange = GC->bUpdateTargetingPresetsOnPawnChange;
	}

	// Optionally update the targeting presets
	if (GC->bUpdateTargetingPresetsOnUpdate)
	{
		UE_LOG(LogGrasp, Verbose, TEXT("%s GraspScanTask::RequestGrasp: Updating targeting presets."),
		       *GetRoleString());
		GC->UpdateTargetingPresets();
	}

	// Get cached targeting presets
	const TMap<FGameplayTag, UTargetingPreset*>& TargetingPresets = GC->CurrentTargetingPresets;

	if (FGraspCVars::bLogVeryVerboseScanRequest)
	{
		UE_LOG(LogGrasp, VeryVerbose, TEXT("%s GraspScanTask::RequestGrasp: TargetingPresets.Num(): %d"), *GetRoleString(),
			   TargetingPresets.Num());
	}
	
	if (TargetingPresets.Num() == 0)
	{
		UE_LOG(LogGrasp, Verbose, TEXT("%s GraspScanTask::RequestGrasp: No targeting presets. [SYSTEM WAIT]"),
		       *GetRoleString());
		WaitForGrasp(Delay, {}, {"No TargetingPresets"});
		return;
	}

	bool bAwaitingCallback = false;
	for (const auto& Entry : TargetingPresets)
	{
		const FGameplayTag& Tag = Entry.Key;
		const UTargetingPreset* Preset = Entry.Value;

		if (!Preset || !Preset->GetTargetingTaskSet() || Preset->GetTargetingTaskSet()->Tasks.IsEmpty())
		{
			// If the only available presets only have empty tasks Grasp will never get a callback
			continue;
		}

		FTargetingRequestHandle& Handle = GC->TargetingRequests.FindOrAdd(Tag);
		Handle = TargetSubsystem->MakeTargetRequestHandle(Preset, FTargetingSourceContext{TargetingSource});

		FTargetingAsyncTaskData& AsyncTaskData = FTargetingAsyncTaskData::FindOrAdd(Handle);
		AsyncTaskData.bReleaseOnCompletion = true;

		bAwaitingCallback = true;

		TargetSubsystem->StartAsyncTargetingRequestWithHandle(Handle,
			FTargetingRequestDelegate::CreateUObject(this, &ThisClass::OnGraspComplete, Tag));

		if (FGraspCVars::bLogVeryVerboseScanRequest)
		{
			UE_LOG(LogGrasp, VeryVerbose,
				   TEXT("%s GraspScanTask::RequestGrasp: Start async targeting for TargetingPresets[%s]: %s"),
				   *GetRoleString(), *Tag.ToString(), *GetNameSafe(Preset));
		}
	}

	if (!bAwaitingCallback)
	{
		// Failed to start any async targeting requests
		UE_LOG(LogGrasp, Verbose,
		       TEXT(
			       "%s GraspScanTask::RequestGrasp: Failed to start async targeting requests - TargetingTaskSet(s) are empty or no Preset assigned! Bad setup! [SYSTEM WAIT]"
		       ), *GetRoleString());
		WaitForGrasp(Delay, {}, {"TargetingTaskSet(s) are empty! Bad setup!"});
		return;
	}

#if UE_ENABLE_DEBUG_DRAWING
	if (IsInGameThread() && GEngine && Ability && Ability->GetCurrentActorInfo())
	{
		if (FGraspCVars::GraspScanDebug > 0)
		{
			const bool bIsLocalPlayer = Ability->GetCurrentActorInfo()->IsLocallyControlled();
			if (FGraspCVars::GraspScanDebug == 1 || bIsLocalPlayer)
			{
				// Draw the number of current requests to screen		
				const int32 UniqueKey = (GC->GetUniqueID() + 297) % INT32_MAX;
				const FString Info = FString::Printf(TEXT("Grasp TargetingRequests: %d"), GC->TargetingRequests.Num());
				GEngine->AddOnScreenDebugMessage(UniqueKey, 5.f, FColor::Green, Info);
			}
		}
	}
#endif
}

void UGraspScanTask::OnGraspComplete(FTargetingRequestHandle TargetingHandle, FGameplayTag ScanTag)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GraspScanTask::OnGraspComplete);

	if (FGraspCVars::bLogVeryVerboseScanRequest)
	{
		UE_LOG(LogGrasp, VeryVerbose, TEXT("%s GraspScanTask::OnGraspComplete: %s"), *GetRoleString(),
			*ScanTag.ToString());
	}

	if (!GC.IsValid())
	{
		UE_LOG(LogGrasp, Verbose, TEXT("%s GraspScanTask::OnGraspComplete: Invalid GraspComponent. [SYSTEM WAIT]"), *GetRoleString());
		WaitForGrasp(Delay, {"Invalid GraspComponent"});
		return;
	}

	// Check if the world and game instance are valid
	if (!GetWorld() || !GetWorld()->GetGameInstance())
	{
		UE_LOG(LogGrasp, Verbose,
		       TEXT("%s GraspScanTask::OnGraspComplete: Invalid world or game instance. [SYSTEM WAIT]"), *GetRoleString());
		
		GC->EndAllTargetingRequests();
		WaitForGrasp(Delay, {}, {"Invalid world or game instance"});
		return;
	}

	// Get the TargetingSubsystem
	const UTargetingSubsystem* TargetSubsystem = GetWorld()->GetGameInstance()->GetSubsystem<UTargetingSubsystem>();
	if (!TargetSubsystem)
	{
		UE_LOG(LogGrasp, Verbose, TEXT("%s GraspScanTask::OnGraspComplete: Invalid TargetingSubsystem. [SYSTEM WAIT]"), *GetRoleString());
		
		GC->EndAllTargetingRequests();
		WaitForGrasp(Delay, {}, {"Invalid TargetingSubsystem"});
		return;
	}

	// Get the results from the TargetingSubsystem
	TArray<FGraspScanResult> ScanResults;
	if (TargetingHandle.IsValid())
	{
		// Process results
		if (FTargetingDefaultResultsSet* Results = FTargetingDefaultResultsSet::Find(TargetingHandle))
		{
			for (FTargetingDefaultResultData& ResultData : Results->TargetResults)
			{
				FHitResult& Hit = ResultData.HitResult;

				// Requires a valid component
				if (!Hit.GetComponent())
				{
					continue;
				}

				const IGraspableComponent* Graspable = CastChecked<IGraspableComponent>(Hit.GetComponent());  // Filtering already checked the type and data
				const FVector Location = Hit.GetComponent()->GetComponentLocation();

				// Calculate the normalized distance
				const float GraspAbilityRadius = Hit.Distance;  // Targeting output the GraspAbilityRadius as Distance
				Hit.Distance = Graspable->GetGraspData()->bGrantAbilityDistance2D ?
					FVector::Dist2D(Location, Hit.TraceStart) :
					FVector::Dist(Location, Hit.TraceStart);
				const float NormalizedDistance = Hit.Distance / GraspAbilityRadius;

				// Add the result to the array
				FGraspScanResult Result = { ScanTag, Hit.GetComponent(), NormalizedDistance };
				ScanResults.Add(Result);
			}
		}

		// Remove the request handle
		GC->TargetingRequests.Remove(ScanTag);
	}

	// Broadcast the results
	if (FGraspCVars::bLogVeryVerboseScanRequest)
	{
		UE_LOG(LogGrasp, VeryVerbose, TEXT("%s GraspScanTask::OnGraspComplete: Broadcasting %d results."), *GetRoleString(),
			   ScanResults.Num());
	}

	GC->GraspTargetsReady(ScanResults);

	// Don't request next grasp if requests are still pending -- otherwise we will re-enter RequestGrasp multiple times
	if (GC->TargetingRequests.Num() == 0)
	{
		// Request the next Grasp
		RequestGrasp();
	}
}

void UGraspScanTask::OnPauseGrasp(bool bPaused)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GraspScanTask::OnPauseGrasp);

	UE_LOG(LogGrasp, Verbose, TEXT("%s GraspScanTask::OnPauseGrasp: %s"), *GetRoleString(),
	       bPaused ? TEXT("Paused") : TEXT("Unpaused"));
	if (bPaused)
	{
		// Cancel the current Grasp
		if (IsValid(GetWorld()))
		{
			GetWorld()->GetTimerManager().ClearTimer(GraspWaitTimer);
		}
	}
	else
	{
		// Request the next Grasp
		RequestGrasp();
	}
}

void UGraspScanTask::OnRequestGrasp()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GraspScanTask::OnRequestGrasp);

	UE_LOG(LogGrasp, Verbose, TEXT("%s GraspScanTask::OnRequestGrasp"), *GetRoleString());

	// GraspComponent ended all our targeting requests and is notifying us to continue
	if (IsValid(GetWorld()))
	{
		// Only continue if we're not already waiting to continue
		if (!GetWorld()->GetTimerManager().IsTimerActive(GraspWaitTimer))
		{
			RequestGrasp();
		}
	}
}

void UGraspScanTask::OnDestroy(bool bInOwnerFinished)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GraspScanTask::OnDestroy);

	if (IsValid(GetWorld()))
	{
		GetWorld()->GetTimerManager().ClearAllTimersForObject(this);

		if (GC.IsValid())
		{
			if (GC->OnPauseGrasp.IsBoundToObject(this))
			{
				GC->OnPauseGrasp.Unbind();
			}
			if (GC->OnRequestGrasp.IsBoundToObject(this))
			{
				GC->OnRequestGrasp.Unbind();
			}
		}
	}

	Super::OnDestroy(bInOwnerFinished);
}

ENetMode UGraspScanTask::GetOwnerNetMode() const
{
	const AActor* const OwnerActorPtr = Ability->GetCurrentActorInfo()->OwnerActor.Get(/*bEvenIfPendingKill=*/ true);
	return OwnerActorPtr ? OwnerActorPtr->GetNetMode() : NM_MAX;
}

FString UGraspScanTask::GetRoleString() const
{
	if (GetOwnerNetMode() == NM_Standalone || GetOwnerNetMode() == NM_MAX)
	{
		return TEXT("");
	}
	return Ability->GetCurrentActorInfo()->IsNetAuthority() ? TEXT("[ Auth ]") : TEXT("[ Client ]");
}
