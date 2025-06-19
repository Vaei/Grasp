// Copyright (c) Jared Taylor


#include "GraspComponent.h"

#include "AbilitySystemComponent.h"
#include "GraspableComponent.h"
#include "GraspData.h"
#include "TargetingSystem/TargetingSubsystem.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Misc/UObjectToken.h"
#include "Components/PrimitiveComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Logging/MessageLog.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GraspComponent)

#define LOCTEXT_NAMESPACE "Grasp"

namespace FGraspCVars
{
#if UE_ENABLE_DEBUG_DRAWING
	// CVar to enable/disable debug drawing
	static bool bGiveAbilityDebug = false;
	FAutoConsoleVariableRef CGiveAbilityDebug(
		TEXT("p.Grasp.GiveAbility.Debug"),
		bGiveAbilityDebug,
		TEXT("Enable debug drawing for Grasp ability give and clear."),
		ECVF_Default);
#endif
}

UGraspComponent::UGraspComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// No ticking or replication, ever
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.bAllowTickOnDedicatedServer = false;
	
	SetIsReplicatedByDefault(false);
}

void UGraspComponent::InitializeGrasp(UAbilitySystemComponent* InAbilitySystemComponent, TSubclassOf<UGameplayAbility> ScanAbility)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GraspComponent::InitializeGrasp);
	
	if (IsValid(GetOwner()))
	{
		// Cache the ability system component
		ASC = InAbilitySystemComponent;

		// Validate the ASC
		if (!ensure(ASC.IsValid()))
		{
			if (IsInGameThread())
			{
				FMessageLog("PIE").Error()
					->AddToken(FUObjectToken::Create(this))
					->AddToken(FTextToken::Create(LOCTEXT("InvalidASC", "Invalid Ability System Component")))
					->AddToken(FTextToken::Create(LOCTEXT("InitializeGrasp", "InitializeGrasp")));
			}
			return;
		}

		// Cache the owning controller
		Controller = Cast<AController>(GetOwner());

		if (GetOwner()->HasAuthority())
		{
			// Pre-grant common grasp abilities
			for (const TSubclassOf<UGameplayAbility>& Ability : CommonGraspAbilities)
			{
				FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(FGameplayAbilitySpec(Ability, 1,
					INDEX_NONE, this));
			
				if (ensure(Handle.IsValid()))
				{
					FGraspAbilityData& Data = AbilityData.FindOrAdd(Ability);
					Data.Handle = Handle;
					Data.Ability = Ability;
					Data.bPersistent = true;  // Don't allow this to be removed

					// Extension point
					PostGiveCommonGraspAbility(Ability, Data);
				}
			}
		
			// End the scan ability if it is already active
			if (ScanAbilityHandle.IsValid())
			{
				ASC->ClearAbility(ScanAbilityHandle);
				ScanAbilityHandle = FGameplayAbilitySpecHandle();
			}

			// Grant the scan ability if provided
			if (ScanAbility)
			{
				ScanAbilityHandle = ASC->GiveAbility(FGameplayAbilitySpec(ScanAbility, 1, INDEX_NONE, this));
			}

			// Cache the preset update mode to detect changed
			bLastUpdateTargetingPresetsOnPawnChange = bUpdateTargetingPresetsOnPawnChange;

			// Get the targeting presets
			if (ScanAbilityHandle.IsValid())
			{
				CurrentTargetingPresets = GetTargetingPresets();
			}

			// Bind the pawn changed event if required
			UpdatePawnChangedBinding();
		}
	}
}

AActor* UGraspComponent::GetTargetingSource_Implementation() const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GraspComponent::GetTargetingSource);
	
	switch (DefaultTargetingSource)
	{
	case EGraspTargetingSource::Pawn: return Controller ? Controller->GetPawn() : nullptr;
	case EGraspTargetingSource::PawnIfValid:
		{
			if (Controller && Controller->GetPawn())
			{
				return Controller->GetPawn();
			}
			return Controller;
		}
	case EGraspTargetingSource::Controller: return Controller;
	}
	return nullptr;
}

TMap<FGameplayTag, UTargetingPreset*> UGraspComponent::GetTargetingPresets_Implementation() const
{
	return DefaultTargetingPresets;
}

const FGraspAbilityData* UGraspComponent::GetGraspAbilityData(const TSubclassOf<UGameplayAbility>& Ability) const
{
	return AbilityData.Find(Ability);
}

void UGraspComponent::UpdatePawnChangedBinding()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GraspComponent::UpdatePawnChangedBinding);

	// We call this if we changed to allow this property to be modified during runtime
	
	if (IsValid(Controller))
	{
		if (Controller->OnPossessedPawnChanged.IsAlreadyBound(this, &ThisClass::OnPawnChanged))
		{
			Controller->OnPossessedPawnChanged.RemoveDynamic(this, &ThisClass::OnPawnChanged);
		}

		// If scanning not enabled then don't bind
		if (ScanAbilityHandle.IsValid())
		{
			if (bUpdateTargetingPresetsOnPawnChange || bEndTargetingRequestsOnPawnChange)
			{
				Controller->OnPossessedPawnChanged.AddDynamic(this, &ThisClass::OnPawnChanged);
			}
		}
	}
}

void UGraspComponent::OnPawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GraspComponent::OnPawnChanged);

	// Optionally end targeting requests
	if (bEndTargetingRequestsOnPawnChange)
	{
		EndAllTargetingRequests();
	}

	// Optionally update targeting presets
	if (bUpdateTargetingPresetsOnPawnChange)
	{
		UpdateTargetingPresets();
	}
}

void UGraspComponent::UpdateTargetingPresets()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GraspComponent::UpdateTargetingPresets);
	
	const TMap<FGameplayTag, UTargetingPreset*> LastTargetingPresets = CurrentTargetingPresets;
	CurrentTargetingPresets = GetTargetingPresets();

	// Clear out any tags that are no longer valid
	for (const auto& Preset : LastTargetingPresets)
	{
		if (!CurrentTargetingPresets.Contains(Preset.Key))
		{
			EndTargetingRequests(Preset.Key);
		}
	}
}

void UGraspComponent::DrawDebugGrantAbilityBox(const UPrimitiveComponent* Component, const FString& Info, const FString& Ability, const FColor& Color) const
{
#if UE_ENABLE_DEBUG_DRAWING
	// Draw a box to show the ability was granted or removed
	if (FGraspCVars::bGiveAbilityDebug)
	{
		DrawDebugBox(GetWorld(), Component->GetComponentLocation(), Component->Bounds.BoxExtent * 2.1f,
			Component->GetComponentQuat(), Color, false, 1.4f,
			SDPG_World, 5.f);
		DrawDebugString(GetWorld(), Component->GetComponentLocation() + FVector(0.f, 0.f, 10.f),
			FString::Printf(TEXT("%s: %s"), *Info, *Ability), nullptr, Color, 1.4f, true);
	}
#endif
}

void UGraspComponent::DrawDebugGrantAbilityLine(const UPrimitiveComponent* Component, const FColor& Color) const
{
#if UE_ENABLE_DEBUG_DRAWING
	// Draw a red box to show the ability was removed
	if (FGraspCVars::bGiveAbilityDebug)
	{
		if (const APawn* Pawn = Controller ? Controller->GetPawn() : nullptr)
		{
			DrawDebugLine(GetWorld(), Pawn->GetActorLocation(), Component->GetComponentLocation(),
				Color, false, GetWorld()->GetDeltaSeconds() * 2.f,
				SDPG_Foreground, 3.f);
		}
	}
#endif
}

void UGraspComponent::GraspTargetsReady(const TArray<FGraspScanResult>& Results)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GraspComponent::GraspTargetsReady);

	if (!HasValidData())
	{
		return;
	}
	
	// Update our current focus results
	TArray<FGraspScanResult> LastScanResults = CurrentScanResults;
	CurrentScanResults = Results;
	
	// Grant any new abilities that aren't pre-granted
	for (const FGraspScanResult& Result : Results)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(GraspComponent::GraspTargetsReady_GrantAbility);
		
		// We have already filtered for these
		const UPrimitiveComponent* Component = Result.Graspable.IsValid() ? Result.Graspable.Get() : nullptr;
		const IGraspableComponent* Graspable = CastChecked<IGraspableComponent>(Component);

		// Ability to grant
		const TSubclassOf<UGameplayAbility>& Ability = Graspable->GetGraspData()->GetGraspAbility();

		// Add ability data
		FGraspAbilityData& Data = AbilityData.FindOrAdd(Ability);

		// This is a common ability, so we don't need to process it
		if (Data.bPersistent)
		{
#if UE_ENABLE_DEBUG_DRAWING || ENABLE_VISUAL_LOG
			DrawDebugGrantAbilityLine(Component, FColor::Purple);
#endif
			continue;
		}

		// This ability is already granted
		if (Data.Handle.IsValid())
		{
			if (!Data.Graspables.Contains(Component))
			{
#if UE_ENABLE_DEBUG_DRAWING || ENABLE_VISUAL_LOG
				DrawDebugGrantAbilityBox(Component, "Retain", GetNameSafe(Ability), FColor::Yellow);
#endif
				Data.Graspables.Add(Component);
			}
#if UE_ENABLE_DEBUG_DRAWING || ENABLE_VISUAL_LOG
			DrawDebugGrantAbilityLine(Component, FColor::Green);
#endif
			continue;
		}

		// Too far away to grant the ability
		const float RequiredDistance = Graspable->GetGraspData()->NormalizedGrantAbilityDistance;
		if (Result.NormalizedScanDistance > RequiredDistance)
		{
#if UE_ENABLE_DEBUG_DRAWING || ENABLE_VISUAL_LOG
			DrawDebugGrantAbilityLine(Component, FColor::Red);

			// Debug text along the line showing how far we are from granting the ability
			if (FGraspCVars::bGiveAbilityDebug)
			{
				const float GrantAbilityPct = 100.f * FMath::Clamp<float>(UKismetMathLibrary::NormalizeToRange(Result.NormalizedScanDistance, RequiredDistance, 1.f), 0.f, 1.f);

				const FVector TextLocation = GetTargetingSource() ? FMath::Lerp<FVector>(Component->GetComponentLocation(),
					GetTargetingSource()->GetActorLocation(), RequiredDistance) : Component->GetComponentLocation();

				DrawDebugString(GetWorld(), TextLocation + FVector(0.f, 0.f, 10.f),
					FString::Printf(TEXT("%.2f%%"), GrantAbilityPct),
					nullptr, FColor::Red, GetWorld()->GetDeltaSeconds() * 2.f, true);
			}
#endif
			
			UE_LOG(LogGrasp, VeryVerbose,
				TEXT("%s GraspComponent::GraspTargetsReady: Not granting ability %s to %s, too far away. NormalizedDistance: %.1f"),
				*GetRoleString(), *Ability->GetName(), *Result.Graspable->GetName(), Result.NormalizedScanDistance);
			continue;
		}

		UE_LOG(LogGrasp, Verbose,
			TEXT("%s GraspComponent::GraspTargetsReady: Granting ability %s to %s"),
			*GetRoleString(), *Ability->GetName(), *Result.Graspable->GetName());

		// Grant the ability
		FGameplayAbilitySpec Spec = FGameplayAbilitySpec(Ability, 1, INDEX_NONE, this);
		FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(Spec);
		
		// Add it to our data
		if (Handle.IsValid())
		{
#if UE_ENABLE_DEBUG_DRAWING || ENABLE_VISUAL_LOG
			DrawDebugGrantAbilityBox(Component, "Give", GetNameSafe(Ability), FColor::Green);
#endif
			
			Data.Handle = Handle;
			Data.Ability = Ability;
			Data.Graspables.Add(Result.Graspable.Get());

			// Extension point
			PostGiveGraspAbility(Ability, Component, Graspable->GetGraspData(), Data);
		}
	}
	
	// Remove any abilities granted for the old results unless they are still valid
	for (const FGraspScanResult& Result : LastScanResults)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(GraspComponent::GraspTargetsReady_RemoveAbility);
		
		// If still valid, don't remove
		if (CurrentScanResults.Contains(Result))  // This compares the Graspable component
		{
			continue;
		}

		// Graspable is no longer valid
		const UPrimitiveComponent* Component = Result.Graspable.IsValid() ? Result.Graspable.Get() : nullptr;
		if (!Component)
		{
			continue;
		}
		const IGraspableComponent* Graspable = CastChecked<IGraspableComponent>(Component);

		// No data to retrieve ability from
		const UGraspData* GraspData = Graspable->GetGraspData();
		if (!GraspData)
		{
			continue;
		}
		
		// If this ability is marked for manual clearing, skip it
		if (GraspData->bManualClearAbility)
		{
			continue;
		}

		// Get the ability to remove
		const TSubclassOf<UGameplayAbility>& Ability = Graspable->GetGraspData()->GetGraspAbility();

		// No ability to remove
		if (!Ability)
		{
			continue;
		}

		// Retrieve the ability data
		FGraspAbilityData* Data = AbilityData.Find(Ability);

		// No ability data, already removed previously during this loop
		if (!Data)
		{
			continue;
		}

		// This is a common ability, so we don't need to process it
		if (Data->bPersistent)
		{
			continue;
		}

		// Clear any weak null ability locks
		Data->LockedGraspables.RemoveAll([](const TWeakObjectPtr<const UPrimitiveComponent>& WeakGraspable)
		{
			return !WeakGraspable.IsValid();
		});

		// If ability lock is in place, skip it
		if (Data->LockedGraspables.Num() > 0)
		{
			continue;
		}

		// Has the ability already been removed?
		if (!Data->Handle.IsValid())
		{
			continue;
		}

		// Are we (partially) responsible for this ability?
		if (Data->Graspables.Contains(Component))
		{
			// Remove our responsibility
			Data->Graspables.Remove(Component);

			// Remove any invalid graspables
			Data->Graspables.RemoveAll([](const TWeakObjectPtr<const UPrimitiveComponent>& WeakGraspable)
			{
				return !WeakGraspable.IsValid();
			});

			UE_LOG(LogGrasp, VeryVerbose,
				TEXT("%s GraspComponent::GraspTargetsReady: Removing ability graspable %s"),
				*GetRoleString(), *Component->GetName());
			
			// If this is the last graspable, remove the ability
			if (Data->Graspables.Num() == 0)
			{
#if UE_ENABLE_DEBUG_DRAWING || ENABLE_VISUAL_LOG
				DrawDebugGrantAbilityBox(Component, "Clear", GetNameSafe(Data->Ability), FColor::Red);
#endif
			
				UE_LOG(LogGrasp, Verbose,
					TEXT("%s GraspComponent::GraspTargetsReady: Removing ability %s"),
					*GetRoleString(), *Ability->GetName());

				// But what if something with the same ability exists in the current results?
				// Cache the result and do it later, but only if still required
				
				PreClearGraspAbility(Ability, Graspable->GetGraspData(), *Data);
				ASC->ClearAbility(Data->Handle);
				AbilityData.Remove(Ability);
			}
#if UE_ENABLE_DEBUG_DRAWING || ENABLE_VISUAL_LOG
			else
			{
				DrawDebugGrantAbilityBox(Component, "Forfeit", GetNameSafe(Ability), FColor::Orange);
			}
#endif
		}
	}
}

void UGraspComponent::PauseGrasp(bool bPaused, bool bEndTargetingRequestsOnPause)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GraspComponent::PauseGrasp);
	
	if (bEndTargetingRequestsOnPause && bPaused)
	{
		EndAllTargetingRequests(false);
	}
	(void)OnPauseGrasp.ExecuteIfBound(bPaused);
}

void UGraspComponent::EndTargetingRequests(const FGameplayTag& PresetTag, bool bNotifyGrasp)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GraspComponent::EndTargetingRequests);
	
	if (!IsValid(GetWorld()) || !IsValid(GetWorld()->GetGameInstance()))
	{
		return;
	}

	if (UTargetingSubsystem* TargetSubsystem = GetWorld()->GetGameInstance()->GetSubsystem<UTargetingSubsystem>())
	{
		// Oddly, there is no 'end all requests' option, and the handles are not accessible, so we track the handles ourselves
		TArray<FGameplayTag> RemovedRequests;
		for (auto& Request : TargetingRequests)
		{
			// If no tag, remove them all
			if (!PresetTag.IsValid() || Request.Key == PresetTag)
			{
				RemovedRequests.Add(Request.Key);
				TargetSubsystem->RemoveAsyncTargetingRequestWithHandle(Request.Value);
			}
		}

		// If all requests were removed, clear the array
		if (RemovedRequests.Num() == TargetingRequests.Num())
		{
			TargetingRequests.Empty();
		}
		else
		{
			for (auto& RemovedRequest : RemovedRequests)
			{
				TargetingRequests.Remove(RemovedRequest);
			}
		}
	}

	// If we removed all requests, call the callback to tell Grasp to update itself
	// It won't receive any callback if there is no pending request, so we need to trigger this
	if (TargetingRequests.Num() == 0 && bNotifyGrasp)
	{
		(void)OnRequestGrasp.ExecuteIfBound();
	}
}

bool UGraspComponent::IsGrantedGameplayAbilityInRange(const TSubclassOf<UGameplayAbility>& InAbility) const
{
	// Anything in current scan results is in range, if it has the ability we are looking for
	for (const FGraspScanResult& Result : CurrentScanResults)
	{
		// We have already filtered for these
		const UPrimitiveComponent* Component = Result.Graspable.IsValid() ? Result.Graspable.Get() : nullptr;
		const IGraspableComponent* Graspable = CastChecked<IGraspableComponent>(Component);

		// Ability to grant via data
		const TSubclassOf<UGameplayAbility>& Ability = Graspable->GetGraspData()->GetGraspAbility();
		if (Ability == InAbility)
		{
			return true;
		}
	}
	return false;
}

bool UGraspComponent::ClearGrantedGameplayAbility(const TSubclassOf<UGameplayAbility>& InAbility, bool bClearAbilitiesInRange,
	bool bClearLockedAbilities)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GraspComponent::ClearGrantedGameplayAbility);
	
	if (!InAbility)
	{
		return false;
	}

	FGraspAbilityData* Data = AbilityData.Find(InAbility);
	if (!Data || !Data->Handle.IsValid())
	{
		// No ability to clear
		return false;
	}

	// If this is a common ability, we don't clear it
	if (Data->bPersistent)
	{
		return false;
	}

	// Don't clear if anything is in range that has this ability
	if (!bClearAbilitiesInRange && IsGrantedGameplayAbilityInRange(InAbility))
	{
		return false;
	}

	// Don't clear locked abilities unless specified
	if (!bClearLockedAbilities)
	{
		// Clear any weak null ability locks
		Data->LockedGraspables.RemoveAll([](const TWeakObjectPtr<const UPrimitiveComponent>& WeakGraspable)
		{
			return !WeakGraspable.IsValid();
		});

		if (Data->LockedGraspables.Num() > 0)
		{
			return false;
		}
	}

	PreClearGraspAbility(InAbility, nullptr, *Data);
	
	ASC->ClearAbility(Data->Handle);
	AbilityData.Remove(InAbility);

	return true;
}

bool UGraspComponent::ClearGrantedGameplayAbilityForComponent(const UPrimitiveComponent* GraspableComponent,
	bool bClearAbilitiesInRange, bool bClearLockedAbilities)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GraspComponent::ClearGrantedGameplayAbilityForComponent);
	
	// We have already filtered for these
	const IGraspableComponent* Graspable = CastChecked<IGraspableComponent>(GraspableComponent);

	// Ability to grant via data
	const TSubclassOf<UGameplayAbility>& Ability = Graspable->GetGraspData()->GetGraspAbility();
	if (Ability)
	{
		return ClearGrantedGameplayAbility(Ability, bClearAbilitiesInRange, bClearLockedAbilities);
	}

	return false;
}

void UGraspComponent::ClearAllGrantedGameplayAbilities(bool bClearCommonAbilities, bool bClearAbilitiesInRange,
	bool bClearLockedAbilities, bool bClearScanAbility, bool bEmptyData)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GraspComponent::ClearAllGrantedGameplayAbilities);
	
	// Clear all granted gameplay abilities
	for (auto& Entry : AbilityData)
	{
		FGraspAbilityData& Data = Entry.Value;

		// Don't clear if anything is in range that has this ability
		if (!bClearAbilitiesInRange && IsGrantedGameplayAbilityInRange(Data.Ability))
		{
			continue;
		}

		// Don't clear locked abilities unless specified
		if (!bClearLockedAbilities)
		{
			// Clear any weak null ability locks
			Data.LockedGraspables.RemoveAll([](const TWeakObjectPtr<const UPrimitiveComponent>& WeakGraspable)
			{
				return !WeakGraspable.IsValid();
			});

			if (Data.LockedGraspables.Num() > 0)
			{
				continue;
			}
		}

		// Don't clear common abilities unless specified
		if (!bClearCommonAbilities && Data.bPersistent)
		{
			continue;
		}
		
		const TWeakObjectPtr<const UPrimitiveComponent>* ValidComponent = Data.Graspables.FindByPredicate(
			[](const TWeakObjectPtr<const UPrimitiveComponent>& Graspable)
			{
				return Graspable.IsValid();
			});
		
		const UGraspData* GraspData = ValidComponent ? CastChecked<IGraspableComponent>(ValidComponent->Get())->GetGraspData() : nullptr;
		PreClearGraspAbility(Data.Ability, GraspData, Data);
		ASC->ClearAbility(Data.Handle);
		Data.Handle = FGameplayAbilitySpecHandle();
		Data.Ability = nullptr;
	}

	// Optionally clear the scan ability too
	if (bClearScanAbility && ScanAbilityHandle.IsValid())
	{
		ASC->ClearAbility(ScanAbilityHandle);
		ScanAbilityHandle = FGameplayAbilitySpecHandle();
	}

	// Can only empty the data if we reset the common abilities also
	if (AbilityData.Num() == 0)
	{
		if (bEmptyData)
		{
			// Empty the array so it releases the allocated memory
			// May cause frame loss
			AbilityData.Empty();
		}
		else
		{
			AbilityData.Reset();
		}
	}
}

bool UGraspComponent::AddAbilityLock(const UPrimitiveComponent* GraspableComponent)
{
	if (!IsValid(GraspableComponent))
	{
		return false;
	}
	
	const IGraspableComponent* Graspable = Cast<IGraspableComponent>(GraspableComponent);
	if (!Graspable || !Graspable->GetGraspData() || !Graspable->GetGraspData()->GetGraspAbility())
	{
		return false;
	}

	// Ability to grant
	const TSubclassOf<UGameplayAbility>& Ability = Graspable->GetGraspData()->GetGraspAbility();

	// Add lock to ability data
	if (FGraspAbilityData* Data = AbilityData.Find(Ability))
	{
		if (!Data->LockedGraspables.Contains(GraspableComponent))
		{
			Data->LockedGraspables.Add(GraspableComponent);
			return true;
		}
	}

	return false;
}

bool UGraspComponent::RemoveAbilityLock(const UPrimitiveComponent* GraspableComponent)
{
	if (!IsValid(GraspableComponent))
	{
		return false;
	}
	
	const IGraspableComponent* Graspable = Cast<IGraspableComponent>(GraspableComponent);
	if (!Graspable || !Graspable->GetGraspData() || !Graspable->GetGraspData()->GetGraspAbility())
	{
		return false;
	}

	// Ability to grant
	const TSubclassOf<UGameplayAbility>& Ability = Graspable->GetGraspData()->GetGraspAbility();

	// Remove lock from ability data
	if (FGraspAbilityData* Data = AbilityData.Find(Ability))
	{
		if (Data->LockedGraspables.Contains(GraspableComponent))
		{
			Data->LockedGraspables.Remove(GraspableComponent);
			return true;
		}
	}

	return false;
}

bool UGraspComponent::HasValidData() const
{
	return IsValid(Controller) && ASC.IsValid();
}

FString UGraspComponent::GetRoleString() const
{
	if (IsValid(GetOwner()))
	{
		return GetOwner()->HasAuthority() ? TEXT("[ Auth ]") : TEXT("[ Client ]");
	}
	return "";
}

#undef LOCTEXT_NAMESPACE