// Copyright (c) Jared Taylor


#include "GraspStatics.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Engine/Engine.h"
#include "GraspableComponent.h"
#include "GraspComponent.h"
#include "GraspData.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/UObjectToken.h"

#if UE_ENABLE_DEBUG_DRAWING
#include "DrawDebugHelpers.h"
#endif

#include "GraspableOwner.h"
#include "Blueprint/SlateBlueprintLibrary.h"
#include "Components/Widget.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GraspStatics)

namespace GraspBaseFrame
{
	/**
	 * A character standing on a moving component has its world location written against that component's pose at carry
	 * time, while a graspable attached to the same component is read at whatever pose that component holds when the
	 * query runs. Comparing the two world locations directly leaks the component's motion between those two instants
	 * into the distance, angle and height checks. Re-express the interactor through the pose the graspable is read at,
	 * so the motion cancels and the query sees the same relationship both ends see.
	 */
	static bool GetInteractorLocationInBaseFrame(const AActor* Interactor, const UPrimitiveComponent* Graspable,
		FVector& OutLocation)
	{
		const ACharacter* Character = Cast<ACharacter>(Interactor);
		if (!Character)
		{
			return false;
		}

		const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
		const FMovementBaseInterfaceData* BaseData = Movement ? Movement->GetMovementBaseInterfaceData() : nullptr;
		const FBasedMovementInfo& BasedMovement = Character->GetBasedMovement();
		if (!MovementBaseUtility::IsMovementBaseDataValid(BaseData) || !MovementBaseUtility::UseRelativeLocation(BaseData))
		{
			return false;
		}

		const AActor* BaseOwner = Cast<AActor>(BaseData->GetMovementBaseObjectOwner());
		const AActor* GraspableOwner = Graspable ? Graspable->GetOwner() : nullptr;
		if (!BaseOwner || !GraspableOwner)
		{
			return false;
		}

		// Only meaningful when the graspable rides the same frame the interactor is standing in.
		if (BaseOwner != GraspableOwner && !GraspableOwner->IsBasedOnActor(BaseOwner))
		{
			return false;
		}

		FVector BaseLocation;
		FQuat BaseQuat;
		if (!MovementBaseUtility::GetMovementBaseTransform(BaseData, BasedMovement.BoneName, BaseLocation, BaseQuat))
		{
			return false;
		}

		OutLocation = BaseLocation + BaseQuat.RotateVector(FVector(BasedMovement.Location));
		return true;
	}
}


FGameplayAbilitySpec* UGraspStatics::FindGraspAbilitySpec(const UAbilitySystemComponent* ASC,
	const UPrimitiveComponent* GraspableComponent, int32 GraspDataIndex)
{
	const IGraspableComponent* Graspable = GraspableComponent ? CastChecked<IGraspableComponent>(GraspableComponent) : nullptr;
	const UGraspData* GraspData = Graspable->GetGraspData(GraspDataIndex);
	if (!GraspData)
	{
		return nullptr;
	}
	const TSubclassOf<UGameplayAbility>& GraspAbility = GraspData->GetGraspAbility();
	return ASC->FindAbilitySpecFromClass(GraspAbility);
}

bool UGraspStatics::PrepareGraspAbilityDataPayload(const UPrimitiveComponent* GraspableComponent,
	FGameplayEventData& Payload, const AActor* SourceActor, const FGameplayAbilityActorInfo* ActorInfo,
	EGraspAbilityComponentSource Source, int32 GraspDataIndex, uint8 EntryState)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GraspStatics::PrepareGraspAbilityDataPayload);
	
	Payload = {};

	// User might handle this in a custom way
	if (Source == EGraspAbilityComponentSource::Custom)
	{
		return false;
	}

	// We would have filtered if the type was invalid
	const IGraspableComponent* Graspable = GraspableComponent ? CastChecked<IGraspableComponent>(GraspableComponent) : nullptr;

	// Gather target data
	TArray<FGameplayAbilityTargetData*> OptionalTargetData = Graspable->GatherOptionalGraspTargetData(ActorInfo);

	// Gather from owner, if implemented
	if (const IGraspableOwner* GraspableOwner = Cast<IGraspableOwner>(GraspableComponent->GetOwner()))
	{
		const TArray<FGameplayAbilityTargetData*> OwnerTargetData = GraspableOwner->GatherOptionalGraspTargetData(ActorInfo);
		if (OwnerTargetData.Num() > 0)
		{
			OptionalTargetData.Append(OwnerTargetData);
		}
	}

	// We may only want to send the target data if we have it
	if (OptionalTargetData.Num() == 0 && Source == EGraspAbilityComponentSource::Automatic)
	{
		return false;
	}

	// Send the component along with the event data
	Payload.OptionalObject = GraspableComponent;

	// Send the specific GraspData entry that triggered this activation
	Payload.OptionalObject2 = Graspable->GetGraspData(GraspDataIndex);

	// Carry the requested entry state so the ability can activate directly into a non-default state
	Payload.EventMagnitude = static_cast<float>(EntryState);

	// Send the target data along with the event data
	for (FGameplayAbilityTargetData* TargetData : OptionalTargetData)
	{
		Payload.TargetData.Add(TargetData);
	}

	return true;
}

const UGraspData* UGraspStatics::GetGraspData(const UPrimitiveComponent* GraspableComponent, int32 Index)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GraspStatics::GetGraspData);

	if (!GraspableComponent)
	{
		return nullptr;
	}

	const IGraspableComponent* Graspable = Cast<IGraspableComponent>(GraspableComponent);
	if (!Graspable)
	{
		return nullptr;
	}

	return Graspable->GetGraspData(Index);
}

static TArray<UGraspData*> EmptyData;
const TArray<UGraspData*>& UGraspStatics::GetGraspDataEntries(const UPrimitiveComponent* GraspableComponent)
{
	if (!GraspableComponent)
	{
		return EmptyData;
	}

	const IGraspableComponent* Graspable = Cast<IGraspableComponent>(GraspableComponent);
	const TArray<TObjectPtr<UGraspData>>* Entries = Graspable ? Graspable->GetGraspDataEntries() : nullptr;
	if (Entries)
	{
		const TArray<UGraspData*>& Decayed = ObjectPtrDecay(*Entries);
		return Decayed;
	}
	return EmptyData;
}

int32 UGraspStatics::GetGraspDataIndex(const UGraspData* GraspData, const UPrimitiveComponent* GraspableComponent)
{
	if (!GraspData || !GraspableComponent)
	{
		return INDEX_NONE;
	}

	const IGraspableComponent* Graspable = Cast<IGraspableComponent>(GraspableComponent);
	if (const TArray<TObjectPtr<UGraspData>>* Entries = Graspable ? Graspable->GetGraspDataEntries() : nullptr)
	{
		return Entries->IndexOfByKey(GraspData);
	}
	return INDEX_NONE;
}

int32 UGraspStatics::GetNumGraspData(const UPrimitiveComponent* GraspableComponent)
{
	if (!GraspableComponent)
	{
		return 0;
	}

	const IGraspableComponent* Graspable = Cast<IGraspableComponent>(GraspableComponent);
	return Graspable ? Graspable->GetNumGraspData() : 0;
}

const UGraspData* UGraspStatics::GetGraspDataFromPayload(const FGameplayEventData& Payload)
{
	return Cast<UGraspData>(Payload.OptionalObject2.Get());
}

bool UGraspStatics::CanGraspActivateAbility(const AActor* SourceActor, const UPrimitiveComponent* GraspableComponent,
	EGraspAbilityComponentSource Source, int32 GraspDataIndex, uint8 EntryState)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GraspStatics::CanGraspActivateAbility);
	
	// Validate the SourceActor
	if (!ensureMsgf(IsValid(SourceActor), TEXT("CanGraspActivateAbility: SourceActor is not valid")))
	{
#if WITH_EDITOR
		FMessageLog("PIE").Error()
			->AddToken(FTextToken::Create(FText::FromString(TEXT("CanGraspActivateAbility: SourceActor is not valid"))));
#endif
		return false;
	}

	// Validate the GraspableComponent
	if (!ensureMsgf(GraspableComponent, TEXT("CanGraspActivateAbility: GraspableComponent is not valid")))
	{
#if WITH_EDITOR
		FMessageLog("PIE").Error()
			->AddToken(FUObjectToken::Create(SourceActor->GetClass()->GetDefaultObject()))
			->AddToken(FTextToken::Create(FText::FromString(TEXT("CanGraspActivateAbility:: GraspableComponent is not valid"))));
#endif
		return false;
	}

	// Find the grasp component (from the SourceActor's Controller)
	UGraspComponent* GraspComponent = FindGraspComponentForActor(SourceActor);
	if (!ensureMsgf(GraspComponent, TEXT("CanGraspActivateAbility: Could not find GraspComponent for SourceActor: %s"), *GetNameSafe(SourceActor)))
	{
#if WITH_EDITOR
		FMessageLog("PIE").Error()
			->AddToken(FUObjectToken::Create(SourceActor->GetClass()->GetDefaultObject()))
			->AddToken(FUObjectToken::Create(GraspableComponent->GetClass()->GetDefaultObject()))
			->AddToken(FTextToken::Create(FText::FromString(TEXT("CanGraspActivateAbility: Could not find GraspComponent for SourceActor"))));
#endif
		return false;
	}

	// Get the ASC from the GraspComponent
	const UAbilitySystemComponent* ASC = GraspComponent->GetASC();
	if (!ASC)
	{
		ASC = GraspFindAbilitySystemComponentForActor(SourceActor);
	}
	if (!ensureMsgf(ASC, TEXT("CanGraspActivateAbility: Could not find AbilitySystemComponent for SourceActor: %s"), *GetNameSafe(SourceActor)))
	{
#if WITH_EDITOR
		FMessageLog("PIE").Error()
			->AddToken(FUObjectToken::Create(SourceActor))
			->AddToken(FUObjectToken::Create(GraspableComponent))
			->AddToken(FTextToken::Create(FText::FromString(TEXT("CanGraspActivateAbility: Could not find AbilitySystemComponent for SourceActor - Did you call InitializeGrasp()?"))));
#endif
		return false;
	}
	
	// Retrieve the ability spec
	const FGameplayAbilitySpec* Spec = FindGraspAbilitySpec(ASC, GraspableComponent, GraspDataIndex);
	if (!Spec || !Spec->Ability)
	{
		return false;
	}

	// Check if we can activate the ability
	const FGameplayAbilityActorInfo* ActorInfo = ASC->AbilityActorInfo.Get();
	FGameplayTagContainer RelevantTags;
	if (Spec->Ability->CanActivateAbility(Spec->Handle, ActorInfo, nullptr, nullptr, &RelevantTags))
	{
		FGameplayEventData Payload;
		if (PrepareGraspAbilityDataPayload(GraspableComponent, Payload, SourceActor, ActorInfo, Source, GraspDataIndex, EntryState))
		{
			return Spec->Ability->ShouldAbilityRespondToEvent(ActorInfo, &Payload);
		}
		return true;
	}
	return false;
}

bool UGraspStatics::TryActivateGraspAbility(const AActor* SourceActor, UPrimitiveComponent* GraspableComponent,
	EGraspAbilityComponentSource Source, int32 GraspDataIndex, uint8 EntryState)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GraspStatics::TryActivateGraspAbility);

	// Validate the SourceActor
	if (!ensureMsgf(IsValid(SourceActor), TEXT("TryActivateGraspAbility: SourceActor is not valid")))
	{
#if WITH_EDITOR
		FMessageLog("PIE").Error()
			->AddToken(FTextToken::Create(FText::FromString(TEXT("TryActivateGraspAbility: SourceActor is not valid"))));
#endif
		return false;
	}

	// Validate the GraspableComponent
	if (!ensureMsgf(GraspableComponent, TEXT("TryActivateGraspAbility: GraspableComponent is not valid")))
	{
#if WITH_EDITOR
		FMessageLog("PIE").Error()
			->AddToken(FUObjectToken::Create(SourceActor->GetClass()->GetDefaultObject()))
			->AddToken(FTextToken::Create(FText::FromString(TEXT("TryActivateGraspAbility:: GraspableComponent is not valid"))));
#endif
		return false;
	}

	// Find the grasp component (from the SourceActor's Controller)
	UGraspComponent* GraspComponent = FindGraspComponentForActor(SourceActor);
	if (!ensureMsgf(GraspComponent, TEXT("TryActivateGraspAbility: Could not find GraspComponent for SourceActor: %s"), *GetNameSafe(SourceActor)))
	{
#if WITH_EDITOR
		FMessageLog("PIE").Error()
			->AddToken(FUObjectToken::Create(SourceActor->GetClass()->GetDefaultObject()))
			->AddToken(FUObjectToken::Create(GraspableComponent->GetClass()->GetDefaultObject()))
			->AddToken(FTextToken::Create(FText::FromString(TEXT("TryActivateGraspAbility: Could not find GraspComponent for SourceActor"))));
#endif
		return false;
	}

	// Get the ASC from the GraspComponent
	UAbilitySystemComponent* ASC = GraspComponent->GetASC();
	if (!ASC)
	{
		ASC = GraspFindAbilitySystemComponentForActor(SourceActor);
	}
	if (!ensureMsgf(ASC, TEXT("TryActivateGraspAbility: Could not find AbilitySystemComponent for SourceActor: %s"), *GetNameSafe(SourceActor)))
	{
#if WITH_EDITOR
		FMessageLog("PIE").Error()
			->AddToken(FUObjectToken::Create(SourceActor))
			->AddToken(FUObjectToken::Create(GraspableComponent))
			->AddToken(FTextToken::Create(FText::FromString(TEXT("TryActivateGraspAbility: Could not find AbilitySystemComponent for SourceActor"))));
#endif
		return false;
	}

	// Get the target component from the target data -- we have already thoroughly validated this elsewhere
	const IGraspableComponent* Graspable = GraspableComponent ? CastChecked<IGraspableComponent>(GraspableComponent) : nullptr;

	// Retrieve the ability spec
	FGameplayAbilitySpec* Spec = FindGraspAbilitySpec(ASC, GraspableComponent, GraspDataIndex);
	if (!Spec || !Spec->Ability)
	{
		return false;
	}

	// Optionally add the input tag to the ability spec
	const UGraspData* GraspDataEntry = Graspable->GetGraspData(GraspDataIndex);
	if (GraspDataEntry && GraspDataEntry->InputTag.IsValid())
	{
		Spec->GetDynamicSpecSourceTags().AddTag(GraspDataEntry->InputTag);
	}

	// Notify
	GraspComponent->PreTryActivateGraspAbility(SourceActor, GraspableComponent, Source, Spec);
	
	// Optional target data
	FGameplayAbilityActorInfo* ActorInfo = ASC->AbilityActorInfo.Get();
	const TArray<FGameplayAbilityTargetData*> OptionalTargetData = Graspable->GatherOptionalGraspTargetData(ActorInfo);
	FGameplayEventData Payload;

	// Prepare the payload
	if (PrepareGraspAbilityDataPayload(GraspableComponent, Payload, SourceActor, ActorInfo, Source, GraspDataIndex, EntryState))
	{
		if (ASC->TriggerAbilityFromGameplayEvent(Spec->Handle, ActorInfo,
			FGraspTags::Grasp_Interact_Activate, &Payload, *ASC))
		{
			GraspComponent->PostActivateGraspAbility(SourceActor, GraspableComponent, Source, Spec, ActorInfo);
			return true;
		}
		else
		{
			GraspComponent->PostFailedActivateGraspAbility(SourceActor, GraspableComponent, Source, Spec, ActorInfo);
			return false;
		}
	}

	// Try to activate the ability
	if (ASC->TryActivateAbility(Spec->Handle, true))
	{
		GraspComponent->PostActivateGraspAbility(SourceActor, GraspableComponent, Source, Spec);
		return true;
	}
	else
	{
		GraspComponent->PostFailedActivateGraspAbility(SourceActor, GraspableComponent, Source, Spec);
		return false;
	}
}

const UObject* UGraspStatics::GetGraspObjectFromPayload(const FGameplayEventData& Payload)
{
	return Payload.OptionalObject;
}

const UPrimitiveComponent* UGraspStatics::K2_GetGraspableComponent(const UGameplayAbility* Ability,
	FGameplayEventData Payload, TSubclassOf<UPrimitiveComponent> ComponentType)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GraspStatics::K2_GetGraspableComponent);

	if (const UObject* Object = GetGraspObjectFromPayload(Payload))
	{
		if (Object->IsA(ComponentType))
		{
			return Cast<UPrimitiveComponent>(Object);
		}
	}
	return nullptr;
}

const UPrimitiveComponent* UGraspStatics::K2_GetGraspablePrimitive(const UGameplayAbility* Ability,
	FGameplayEventData Payload)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GraspStatics::K2_GetGraspablePrimitive);

	if (const UObject* Object = GetGraspObjectFromPayload(Payload))
	{
		return Cast<UPrimitiveComponent>(Object);
	}
	return nullptr;
}

UAbilitySystemComponent* UGraspStatics::GraspFindAbilitySystemComponentForActor(const AActor* Actor)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GraspStatics::GraspFindAbilitySystemComponentForActor);

	if (!IsValid(Actor))
	{
		return nullptr;
	}

	// CPP only
	const IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(Actor);
	if (ASI)
	{
		return ASI->GetAbilitySystemComponent();
	}

	// Maybe the actor is a pawn
	if (const APawn* Pawn = Cast<APawn>(Actor))
	{
		// And maybe the ASC is on the pawn
		if (UAbilitySystemComponent* ASC = Pawn->FindComponentByClass<UAbilitySystemComponent>())
		{
			return ASC;
		}

		// Maybe the ASC is on the player state
		if (const APlayerState* PlayerState = Pawn->GetPlayerState())
		{
			return PlayerState->FindComponentByClass<UAbilitySystemComponent>();
		}
	}

	return nullptr;
}

UGraspComponent* UGraspStatics::FindGraspComponentForActor(const AActor* Actor)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GraspStatics::FindGraspComponentForActor);

	if (!IsValid(Actor))
	{
		return nullptr;
	}

	// Only Local and Authority has a Controller
	if (Actor->GetLocalRole() == ROLE_SimulatedProxy)
	{
		return nullptr;
	}

	// Maybe the actor is a controller
	if (const AController* Controller = Cast<AController>(Actor))
	{
		return Controller->FindComponentByClass<UGraspComponent>();
	}

	// Maybe the actor is a pawn
	if (const APawn* Pawn = Cast<APawn>(Actor))
	{
		if (const AController* Controller = Pawn->GetController())
		{
			return Controller->FindComponentByClass<UGraspComponent>();
		}
	}

	// Maybe the actor is a player state
	if (const APlayerState* PlayerState = Cast<APlayerState>(Actor))
	{
		if (const AController* Controller = PlayerState->GetOwningController())
		{
			return Controller->FindComponentByClass<UGraspComponent>();
		}
	}

	// Unsupported actor
	return nullptr;
}

UGraspComponent* UGraspStatics::FindGraspComponentForPawn(APawn* Pawn)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GraspStatics::FindGraspComponentForPawn);
	
	if (!IsValid(Pawn))
	{
		return nullptr;
	}
	
	// Only Local and Authority has a Controller
	if (Pawn->GetLocalRole() == ROLE_SimulatedProxy)
	{
		return nullptr;
	}
	
	if (const AController* Controller = Pawn->GetController())
	{
		return Controller->FindComponentByClass<UGraspComponent>();
	}
	return nullptr;
}

UGraspComponent* UGraspStatics::FindGraspComponentForController(AController* Controller)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GraspStatics::FindGraspComponentForController);
	
	if (!IsValid(Controller))
	{
		return nullptr;
	}
	
	// Only Local and Authority has a Controller
	if (Controller->GetLocalRole() == ROLE_SimulatedProxy)
	{
		return nullptr;
	}
	
	return Controller->FindComponentByClass<UGraspComponent>();
}

UGraspComponent* UGraspStatics::FindGraspComponentForPlayerState(APlayerState* PlayerState)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GraspStatics::FindGraspComponentForPlayerState);
	
	if (!IsValid(PlayerState))
	{
		return nullptr;
	}
	
	// Only Local and Authority has a Controller
	if (PlayerState->GetLocalRole() == ROLE_SimulatedProxy)
	{
		return nullptr;
	}
	
	if (const AController* Controller = PlayerState->GetOwningController())
	{
		return Controller->FindComponentByClass<UGraspComponent>();
	}
	return nullptr;
}

bool UGraspStatics::AddGraspAbilityLock(const AActor* SourceActor, const UPrimitiveComponent* GraspableComponent)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GraspStatics::AddGraspAbilityLock);

	// Validate the SourceActor
	if (!ensureMsgf(IsValid(SourceActor), TEXT("TryActivateGraspAbility: SourceActor is not valid")))
	{
#if WITH_EDITOR
		FMessageLog("PIE").Error()
			->AddToken(FTextToken::Create(FText::FromString(TEXT("TryActivateGraspAbility: SourceActor is not valid"))));
#endif
		return false;
	}

	// Validate the GraspableComponent
	if (!ensureMsgf(GraspableComponent, TEXT("TryActivateGraspAbility: GraspableComponent is not valid")))
	{
#if WITH_EDITOR
		FMessageLog("PIE").Error()
			->AddToken(FUObjectToken::Create(SourceActor->GetClass()->GetDefaultObject()))
			->AddToken(FTextToken::Create(FText::FromString(TEXT("TryActivateGraspAbility:: GraspableComponent is not valid"))));
#endif
		return false;
	}

	// Find the grasp component (from the SourceActor's Controller)
	UGraspComponent* GraspComponent = FindGraspComponentForActor(SourceActor);
	if (!ensureMsgf(GraspComponent, TEXT("TryActivateGraspAbility: Could not find GraspComponent for SourceActor: %s"), *GetNameSafe(SourceActor)))
	{
#if WITH_EDITOR
		FMessageLog("PIE").Error()
			->AddToken(FUObjectToken::Create(SourceActor->GetClass()->GetDefaultObject()))
			->AddToken(FUObjectToken::Create(GraspableComponent->GetClass()->GetDefaultObject()))
			->AddToken(FTextToken::Create(FText::FromString(TEXT("TryActivateGraspAbility: Could not find GraspComponent for SourceActor"))));
#endif
		return false;
	}

	return GraspComponent->AddAbilityLock(GraspableComponent);
}

bool UGraspStatics::RemoveGraspAbilityLock(const AActor* SourceActor, const UPrimitiveComponent* GraspableComponent)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GraspStatics::RemoveGraspAbilityLock);

	// Validate the SourceActor
	if (!ensureMsgf(IsValid(SourceActor), TEXT("TryActivateGraspAbility: SourceActor is not valid")))
	{
#if WITH_EDITOR
		FMessageLog("PIE").Error()
			->AddToken(FTextToken::Create(FText::FromString(TEXT("TryActivateGraspAbility: SourceActor is not valid"))));
#endif
		return false;
	}

	// Validate the GraspableComponent
	if (!ensureMsgf(GraspableComponent, TEXT("TryActivateGraspAbility: GraspableComponent is not valid")))
	{
#if WITH_EDITOR
		FMessageLog("PIE").Error()
			->AddToken(FUObjectToken::Create(SourceActor->GetClass()->GetDefaultObject()))
			->AddToken(FTextToken::Create(FText::FromString(TEXT("TryActivateGraspAbility:: GraspableComponent is not valid"))));
#endif
		return false;
	}

	// Find the grasp component (from the SourceActor's Controller)
	UGraspComponent* GraspComponent = FindGraspComponentForActor(SourceActor);
	if (!ensureMsgf(GraspComponent, TEXT("TryActivateGraspAbility: Could not find GraspComponent for SourceActor: %s"), *GetNameSafe(SourceActor)))
	{
#if WITH_EDITOR
		FMessageLog("PIE").Error()
			->AddToken(FUObjectToken::Create(SourceActor->GetClass()->GetDefaultObject()))
			->AddToken(FUObjectToken::Create(GraspableComponent->GetClass()->GetDefaultObject()))
			->AddToken(FTextToken::Create(FText::FromString(TEXT("TryActivateGraspAbility: Could not find GraspComponent for SourceActor"))));
#endif
		return false;
	}

	return GraspComponent->RemoveAbilityLock(GraspableComponent);
}

void UGraspStatics::FlushServerMovesForActor(AActor* CharacterActor)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GraspStatics::FlushServerMovesForActor);
	
	if (IsValid(CharacterActor))
	{
		if (const ACharacter* Character = Cast<ACharacter>(CharacterActor))
		{
			if (UCharacterMovementComponent* CharacterMovement = Character->GetCharacterMovement())
			{
				CharacterMovement->FlushServerMoves();
			}
		}
	}
}

void UGraspStatics::FlushServerMoves(ACharacter* Character)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GraspStatics::FlushServerMoves);
	
	if (IsValid(Character))
	{
		if (UCharacterMovementComponent* CharacterMovement = Character->GetCharacterMovement())
		{
			CharacterMovement->FlushServerMoves();
		}
	}
}

EGraspCardinal_4Way UGraspStatics::GetCardinalDirectionFromAngle_4Way(float Angle)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GraspStatics::GetCardinalDirectionFromAngle_4Way);
	
	const float AngleAbs = FMath::Abs(Angle);

	// Forward
	if (AngleAbs <= 45.f)
	{
		return EGraspCardinal_4Way::Forward;
	}

	// Backward
	if (AngleAbs >= 135.f)
	{
		return EGraspCardinal_4Way::Backward;
	}

	// Right
	if (Angle > 0.f)
	{
		return EGraspCardinal_4Way::Right;
	}

	// Left
	return EGraspCardinal_4Way::Left;
}

EGraspCardinal_8Way UGraspStatics::GetCardinalDirectionFromAngle_8Way(float Angle)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GraspStatics::GetCardinalDirectionFromAngle_8Way);
	
	const float AngleAbs = FMath::Abs(Angle);

	// Forward
	if (AngleAbs <= 22.5f)
	{
		return EGraspCardinal_8Way::Forward;
	}

	// Backward
	if (AngleAbs >= 157.5f)
	{
		return EGraspCardinal_8Way::Backward;
	}

	// Diagonal Fwd
	if (AngleAbs <= 67.5f)
	{
		return Angle > 0.f ? EGraspCardinal_8Way::ForwardRight : EGraspCardinal_8Way::ForwardLeft;
	}

	// Diagonal Bwd
	if (AngleAbs >= 112.5f)
	{
		return Angle > 0.f ? EGraspCardinal_8Way::BackwardRight : EGraspCardinal_8Way::BackwardLeft;
	}

	// Right
	if (Angle > 0.f)
	{
		return EGraspCardinal_8Way::Right;
	}

	// Left
	return EGraspCardinal_8Way::Left;
}

float UGraspStatics::CalculateCardinalAngle(const FVector& Direction, const FRotator& SourceRotation)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GraspStatics::CalculateCardinalAngle);
	
	// Copied from UKismetAnimationLibrary::CalculateDirection
	
	if (!Direction.IsNearlyZero())
	{
		const FMatrix RotMatrix = FRotationMatrix(SourceRotation);
		const FVector ForwardVector = RotMatrix.GetScaledAxis(EAxis::X);
		const FVector RightVector = RotMatrix.GetScaledAxis(EAxis::Y);
		const FVector Normalize = Direction.GetSafeNormal2D();

		// get a cos(alpha) of forward vector vs velocity
		const float ForwardCosAngle = static_cast<float>(FVector::DotProduct(ForwardVector, Normalize));
		// now get the alpha and convert to degree
		float ForwardDeltaDegree = FMath::RadiansToDegrees(FMath::Acos(ForwardCosAngle));

		// depending on where right vector is, flip it
		const float RightCosAngle = static_cast<float>(FVector::DotProduct(RightVector, Normalize));
		if (RightCosAngle < 0.f)
		{
			ForwardDeltaDegree *= -1.f;
		}

		return ForwardDeltaDegree;
	}

	return 0.f;
}

EGraspCardinal_4Way UGraspStatics::CalculateCardinalDirection_4Way(const FVector& SourceLocation,
	const FRotator& SourceRotation,	const FVector& TargetLocation)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GraspStatics::CalculateCardinalDirection_4Way);
	
	const FVector Direction = TargetLocation - SourceLocation;
	const float Angle = CalculateCardinalAngle(Direction, SourceRotation);
	return GetCardinalDirectionFromAngle_4Way(Angle);
}

EGraspCardinal_8Way UGraspStatics::CalculateCardinalDirection_8Way(const FVector& SourceLocation,
	const FRotator& SourceRotation,	const FVector& TargetLocation)
{ 
	TRACE_CPUPROFILER_EVENT_SCOPE(GraspStatics::CalculateCardinalDirection_8Way);
	
	const FVector Direction = TargetLocation - SourceLocation;
	const float Angle = CalculateCardinalAngle(Direction, SourceRotation);
	return GetCardinalDirectionFromAngle_8Way(Angle);
}

EGraspCardinal_4Way UGraspStatics::GetOppositeCardinalDirection_4Way(EGraspCardinal_4Way Cardinal)
{
	switch (Cardinal)
	{
	case EGraspCardinal_4Way::Forward: return EGraspCardinal_4Way::Backward;
	case EGraspCardinal_4Way::Backward: return EGraspCardinal_4Way::Forward;
	case EGraspCardinal_4Way::Left: return EGraspCardinal_4Way::Right;
	case EGraspCardinal_4Way::Right: return EGraspCardinal_4Way::Left;
	default: return Cardinal;
	}
}

EGraspCardinal_8Way UGraspStatics::GetOppositeCardinalDirection_8Way(EGraspCardinal_8Way Cardinal)
{
	switch (Cardinal)
	{
	case EGraspCardinal_8Way::Forward: return EGraspCardinal_8Way::Backward;
	case EGraspCardinal_8Way::Backward: return EGraspCardinal_8Way::Forward;
	case EGraspCardinal_8Way::Left: return EGraspCardinal_8Way::Right;
	case EGraspCardinal_8Way::Right: return EGraspCardinal_8Way::Left;
	case EGraspCardinal_8Way::ForwardLeft: return EGraspCardinal_8Way::BackwardRight;
	case EGraspCardinal_8Way::ForwardRight: return EGraspCardinal_8Way::BackwardLeft;
	case EGraspCardinal_8Way::BackwardLeft: return EGraspCardinal_8Way::ForwardRight;
	case EGraspCardinal_8Way::BackwardRight: return EGraspCardinal_8Way::ForwardLeft;
	default: return Cardinal;
	}
}

FVector UGraspStatics::GetDirectionFromCardinal_4Way(EGraspCardinal_4Way Cardinal, const FRotator& SourceRotation)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GraspStatics::GetDirectionFromCardinal_4Way);
	return SourceRotation.RotateVector(GetSnappedDirectionFromCardinal_4Way(Cardinal));
}

FVector UGraspStatics::GetDirectionFromCardinal_8Way(EGraspCardinal_8Way Cardinal, const FRotator& SourceRotation)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GraspStatics::GetDirectionFromCardinal_8Way);
	return SourceRotation.RotateVector(GetSnappedDirectionFromCardinal_8Way(Cardinal));
}

FVector UGraspStatics::GetSnappedDirectionFromCardinal_4Way(EGraspCardinal_4Way Cardinal)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GraspStatics::GetSnappedDirectionFromCardinal_4Way);
	switch (Cardinal)
	{
	case EGraspCardinal_4Way::Forward:
		return FVector(1.f, 0.f, 0.f);
	case EGraspCardinal_4Way::Backward:
		return FVector(-1.f, 0.f, 0.f);
	case EGraspCardinal_4Way::Left:
		return FVector(0.f, -1.f, 0.f);
	case EGraspCardinal_4Way::Right:
		return FVector(0.f, 1.f, 0.f);
	default:
		return FVector::ZeroVector;
	}
}

FVector UGraspStatics::GetSnappedDirectionFromCardinal_8Way(EGraspCardinal_8Way Cardinal)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GraspStatics::GetSnappedDirectionFromCardinal_8Way);
	switch (Cardinal)
	{
	case EGraspCardinal_8Way::Forward:
		return FVector(1.f, 0.f, 0.f);
	case EGraspCardinal_8Way::Backward:
		return FVector(-1.f, 0.f, 0.f);
	case EGraspCardinal_8Way::Left:
		return FVector(0.f, -1.f, 0.f);
	case EGraspCardinal_8Way::Right:
		return FVector(0.f, 1.f, 0.f);
	case EGraspCardinal_8Way::ForwardLeft:
		return FVector(1.f, -1.f, 0.f).GetSafeNormal2D();
	case EGraspCardinal_8Way::ForwardRight:
		return FVector(1.f, 1.f, 0.f).GetSafeNormal2D();
	case EGraspCardinal_8Way::BackwardLeft:
		return FVector(-1.f, -1.f, 0.f).GetSafeNormal2D();
	case EGraspCardinal_8Way::BackwardRight:
		return FVector(-1.f, 1.f, 0.f).GetSafeNormal2D();
	default:
		return FVector::ZeroVector;
	}
}

FVector UGraspStatics::GetDirectionSnappedToCardinal(const FVector& SourceLocation, const FRotator& SourceRotation,
	const FVector& TargetLocation, EGraspCardinalType CardinalType, bool bFlipDirection)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GraspStatics::GetDirectionSnappedToCardinal);
	
	const FVector Direction = TargetLocation - SourceLocation;
	const float Angle = CalculateCardinalAngle(Direction, SourceRotation);

	switch (CardinalType)
	{
	case EGraspCardinalType::Cardinal_8Way:
		{
			EGraspCardinal_8Way Cardinal = GetCardinalDirectionFromAngle_8Way(Angle);
			if (bFlipDirection)
			{
				Cardinal = GetOppositeCardinalDirection_8Way(Cardinal);
			}
			return GetDirectionFromCardinal_8Way(Cardinal, SourceRotation);
		}
	default:
		{
			EGraspCardinal_4Way Cardinal = GetCardinalDirectionFromAngle_4Way(Angle);
			if (bFlipDirection)
			{
				Cardinal = GetOppositeCardinalDirection_4Way(Cardinal);
			}
			return GetDirectionFromCardinal_4Way(Cardinal, SourceRotation);
		}
	}
}

namespace
{
	/** Distance between A and B on the plane perpendicular to Up */
	float GraspPlanarDist(const FVector& A, const FVector& B, const FVector& Up)
	{
		return FVector::VectorPlaneProject(B - A, Up).Size();
	}

	float GraspPlanarDistSquared(const FVector& A, const FVector& B, const FVector& Up)
	{
		return FVector::VectorPlaneProject(B - A, Up).SizeSquared();
	}
}

FVector UGraspStatics::GetGraspUpVector(EGraspUpMode UpMode, const UPrimitiveComponent* Graspable, FVector CustomUp)
{
	switch (UpMode)
	{
	case EGraspUpMode::GraspableUp:
		return Graspable ? Graspable->GetUpVector() : FVector::UpVector;
	case EGraspUpMode::GraspableOwnerUp:
		return Graspable && Graspable->GetOwner() ? Graspable->GetOwner()->GetActorUpVector() : FVector::UpVector;
	case EGraspUpMode::CustomUp:
		{
			const FVector Normalized = CustomUp.GetSafeNormal();
			return Normalized.IsNearlyZero() ? FVector::UpVector : Normalized;
		}
	case EGraspUpMode::WorldUp:
	default:
		return FVector::UpVector;
	}
}

bool UGraspStatics::IsWithinInteractAngle(const FVector& InteractorLocation, const FVector& InteractableLocation, const FVector& Forward, float Degrees, bool bCheck2D, bool
	bHalfCircle, FVector Up)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GraspStatics::IsWithinInteractAngle);

	const FVector Diff = InteractableLocation - InteractorLocation;
	const FVector Dir = bCheck2D ? FVector::VectorPlaneProject(Diff, Up).GetSafeNormal() : Diff.GetSafeNormal();
	const float Radians = FMath::DegreesToRadians(Degrees * (bHalfCircle ? 1.f : 0.5f));
	const float Acos = FMath::Acos(Forward | Dir);
	return Acos <= Radians;
}

bool UGraspStatics::IsInteractableWithinAngle(const FVector& InteractorLocation, const FVector& InteractableLocation,
	const FVector& Forward, float Degrees, FVector Up)
{
	return IsWithinInteractAngle(InteractorLocation, InteractableLocation,
		Forward, Degrees, true, false, Up);
}

FVector UGraspStatics::GetGraspableForwardVectorFromTransform(const FTransform& Transform, EGraspForwardAxis Axis,
	float YawOffset)
{
	// Resolve the local forward from the authored forward-axis convention
	FVector LocalForward;
	switch (Axis)
	{
	case EGraspForwardAxis::NegX:	LocalForward = -FVector::ForwardVector;	break;	// (-1, 0, 0)
	case EGraspForwardAxis::PosY:	LocalForward = FVector::RightVector;	break;	// ( 0, 1, 0)
	case EGraspForwardAxis::NegY:	LocalForward = -FVector::RightVector;	break;	// ( 0,-1, 0)
	case EGraspForwardAxis::PosX:
	default:						LocalForward = FVector::ForwardVector;	break;	// ( 1, 0, 0)
	}

	// Apply the optional yaw offset about the local up axis, decoupling the grasp facing
	// from the component's own rotation. Done in local space so it composes with the axis
	// remap and stays correct for graspables that pitch/roll (e.g. mounted on a ship).
	if (!FMath::IsNearlyZero(YawOffset))
	{
		LocalForward = FRotator(0.f, YawOffset, 0.f).RotateVector(LocalForward);
	}

	return Transform.TransformVectorNoScale(LocalForward);
}

FVector UGraspStatics::GetGraspableForwardVector(const UPrimitiveComponent* Graspable, const UGraspData* Data)
{
	if (!Graspable)
	{
		return FVector::ForwardVector;
	}

	const IGraspableComponent* IGraspable = Cast<IGraspableComponent>(Graspable);
	const EGraspForwardAxis Axis = IGraspable ? IGraspable->GetGraspableForwardAxis() : EGraspForwardAxis::PosX;

	// Compound the component-level yaw offset with the per-data offset (if any) so a single
	// graspable can host multiple GraspData entries whose interaction arcs face different directions
	float YawOffset = IGraspable ? IGraspable->GetGraspableYawOffset() : 0.f;
	if (Data)
	{
		YawOffset += Data->GetGraspableYawOffset();
	}
	return GetGraspableForwardVectorFromTransform(Graspable->GetComponentTransform(), Axis, YawOffset);
}

bool UGraspStatics::CanInteractWithinAngle(const AActor* Interactor, const FVector& InteractableLocation, float Degrees,
	FVector Up)
{
	if (!IsValid(Interactor))
	{
		return false;
	}
	return IsInteractableWithinAngle(InteractableLocation, Interactor->GetActorLocation(),
		Interactor->GetActorForwardVector(), Degrees, Up);
}

bool UGraspStatics::IsWithinInteractDistance(const FVector& InteractorLocation, const FVector& InteractableLocation,
	float Distance, bool bCheck2D, FVector Up)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GraspStatics::IsWithinInteractDistance);

	const float DistSquared = bCheck2D ?
		GraspPlanarDistSquared(InteractorLocation, InteractableLocation, Up) : FVector::DistSquared(InteractorLocation, InteractableLocation);
	return DistSquared <= FMath::Square(Distance);
}

bool UGraspStatics::IsInteractableWithinDistance(const FVector& InteractorLocation, const FVector& InteractableLocation,
	float Distance, bool bCheck2D, FVector Up)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GraspStatics::IsInteractableWithinDistance);

	return IsWithinInteractDistance(InteractorLocation, InteractableLocation,
		Distance, bCheck2D, Up);
}

bool UGraspStatics::CanInteractWithinDistance(const AActor* Interactor, const FVector& InteractableLocation,
	float Distance, bool bCheck2D, FVector Up)
{
	if (!IsValid(Interactor))
	{
		return false;
	}
	return IsInteractableWithinDistance(InteractableLocation, Interactor->GetActorLocation(), Distance, bCheck2D, Up);
}

bool UGraspStatics::CanInteractWithinAngleAndDistance(const AActor* Interactor, const FVector& InteractableLocation,
	float Degrees, float Distance, FVector Up)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GraspStatics::CanInteractWithinAngleAndDistance);

	if (!IsValid(Interactor))
	{
		return false;
	}

	const bool bWithinAngle = IsInteractableWithinAngle(InteractableLocation,
		Interactor->GetActorLocation(), Interactor->GetActorForwardVector(), Degrees, Up);

	const bool bWithinDistance = IsInteractableWithinDistance(InteractableLocation,
		Interactor->GetActorLocation(), Distance, true, Up);

	return bWithinAngle && bWithinDistance;
}

bool UGraspStatics::IsInteractableWithinHeight(const FVector& InteractorLocation, const FVector& InteractableLocation,
	float MaxHeightAbove, float MaxHeightBelow, FVector Up)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GraspStatics::IsInteractableWithinHeight);

	const float Height = (InteractableLocation - InteractorLocation) | Up;
	return Height >= -MaxHeightBelow && Height <= MaxHeightAbove;
}

bool UGraspStatics::CanInteractWithinHeight(const AActor* Interactor, const FVector& InteractableLocation,
	float MaxHeightAbove, float MaxHeightBelow, FVector Up)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GraspStatics::CanInteractWithinHeight);

	if (!IsValid(Interactor))
	{
		return false;
	}
	return IsInteractableWithinHeight(InteractableLocation, Interactor->GetActorLocation(),
		MaxHeightAbove, MaxHeightBelow, Up);
}

EGraspQueryResult UGraspStatics::CanInteractWith(const AActor* Interactor, const UPrimitiveComponent* Component,
	float& NormalizedAngleDiff, float& NormalizedDistance, float& NormalizedHighlightDistance,
	int32 GraspDataIndex, EGraspUpMode UpMode, FVector CustomUp)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GraspStatics::CanInteractWith);
	
	NormalizedAngleDiff = 0.f;
	NormalizedDistance = 0.f;
	NormalizedHighlightDistance = 0.f;

	// Validate the interactor
	if (!IsValid(Interactor))
	{
		return EGraspQueryResult::None;
	}

	// Validate the graspable
	if (!Component)
	{
		return EGraspQueryResult::None;
	}

	// Validate the grasp data
	const IGraspableComponent* Graspable = CastChecked<IGraspableComponent>(Component);
	const UGraspData* Data = Graspable->GetGraspData(GraspDataIndex);
	if (!ensure(Data != nullptr))
	{
		return EGraspQueryResult::None;
	}

	FVector InteractorLocation = Interactor->GetActorLocation();
	GraspBaseFrame::GetInteractorLocationInBaseFrame(Interactor, Component, InteractorLocation);

	const FVector Location = Component->GetComponentLocation();
	const FVector Forward = GetGraspableForwardVector(Component, Data);
	const FVector Up = GetGraspUpVector(UpMode, Component, CustomUp);

	const float AuthNetToleranceAngleScalar = Data->GetAuthNetToleranceAngleScalar();
	const float AuthNetToleranceDistanceScalar = Data->GetAuthNetToleranceDistanceScalar();
	const bool bApplyAuthScalar = Interactor->HasAuthority() && Interactor->GetNetMode() != NM_Standalone;

	const float BaseAngle = Data->GetMaxGraspAngle(Interactor);
	const float Angle = bApplyAuthScalar ? BaseAngle * AuthNetToleranceAngleScalar : BaseAngle;

	const float BaseDistance = Data->GetMaxGraspDistance(Interactor);
	const float Distance = bApplyAuthScalar ? BaseDistance * AuthNetToleranceDistanceScalar : BaseDistance;

	const float HighlightDistance = bApplyAuthScalar ?
		Data->MaxHighlightDistance * AuthNetToleranceDistanceScalar : Data->MaxHighlightDistance;

	const float BaseHeightAbove = Data->GetMaxHeightAbove(Interactor);
	const float MaxHeightAbove = bApplyAuthScalar ? BaseHeightAbove * AuthNetToleranceDistanceScalar : BaseHeightAbove;

	const float BaseHeightBelow = Data->GetMaxHeightBelow(Interactor);
	const float MaxHeightBelow = bApplyAuthScalar ? BaseHeightBelow * AuthNetToleranceDistanceScalar : BaseHeightBelow;
	
	// Check if within distance
	if (!IsInteractableWithinDistance(Location, InteractorLocation, Distance, true, Up))
	{
		// Check if highlight is enabled and within distance
		if (HighlightDistance > 0.f && IsInteractableWithinDistance(Location, InteractorLocation, HighlightDistance, true, Up))
		{
			NormalizedHighlightDistance = FMath::Clamp(
				GraspPlanarDist(Location, InteractorLocation, Up) / HighlightDistance, 0.f, 1.f);

			// We sorted by distance, if this one is too far, the rest are too
			return EGraspQueryResult::Highlight;
		}

		// We sorted by distance, if this one is too far, the rest are too
		return EGraspQueryResult::None;
	}

	const float DistNormalized = Data->IsGraspDistance2D(Interactor) ? GraspPlanarDist(Location, InteractorLocation, Up) :
		FVector::Dist(Location, InteractorLocation);
	NormalizedDistance = FMath::Clamp(DistNormalized / Distance, 0.f, 1.f);

	// Check if within angle
	if (!IsInteractableWithinAngle(Location, InteractorLocation, Forward, Angle, Up))
	{
		return EGraspQueryResult::None;
	}

	NormalizedAngleDiff = FMath::Clamp(
		GraspPlanarDist(Location, InteractorLocation, Up) / Angle, 0.f, 1.f);

	// Check if within height
	if (!IsInteractableWithinHeight(Location, InteractorLocation, MaxHeightAbove, MaxHeightBelow, Up))
	{
		return EGraspQueryResult::None;
	}

	return EGraspQueryResult::Interact;
}

EGraspQueryResult UGraspStatics::CanInteractWithRange(const AActor* Interactor, const UPrimitiveComponent* Graspable,
	float& NormalizedDistance, float& NormalizedHighlightDistance,
	int32 GraspDataIndex, EGraspUpMode UpMode, FVector CustomUp)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GraspStatics::CanInteractWithRange);

	NormalizedDistance = 0.f;
	NormalizedHighlightDistance = 0.f;

	// Validate the interactor
	if (!IsValid(Interactor))
	{
		return EGraspQueryResult::None;
	}

	// Validate the graspable
	if (!Graspable)
	{
		return EGraspQueryResult::None;
	}

	FVector InteractorLocation = Interactor->GetActorLocation();
	GraspBaseFrame::GetInteractorLocationInBaseFrame(Interactor, Graspable, InteractorLocation);

	const FVector Location = Graspable->GetComponentLocation();
	const UGraspData* Data = CastChecked<IGraspableComponent>(Graspable)->GetGraspData(GraspDataIndex);
	if (!Data)
	{
		return EGraspQueryResult::None;
	}

	const float AuthNetToleranceDistanceScalar = Data->GetAuthNetToleranceDistanceScalar();
	const bool bApplyAuthScalar = Interactor->HasAuthority() && Interactor->GetNetMode() != NM_Standalone;

	const float BaseDistance = Data->GetMaxGraspDistance(Interactor);
	const float Distance = bApplyAuthScalar ? BaseDistance * AuthNetToleranceDistanceScalar : BaseDistance;

	const float HighlightDistance = bApplyAuthScalar ?
		Data->MaxHighlightDistance * AuthNetToleranceDistanceScalar : Data->MaxHighlightDistance;

	const FVector Up = GetGraspUpVector(UpMode, Graspable, CustomUp);

	// Check if within distance
	if (!IsInteractableWithinDistance(Location, InteractorLocation, Distance, true, Up))
	{
		// Check if highlight is enabled and within distance
		if (HighlightDistance > 0.f && IsInteractableWithinDistance(Location, InteractorLocation, HighlightDistance, true, Up))
		{
			NormalizedHighlightDistance = FMath::Clamp(
				GraspPlanarDist(Location, InteractorLocation, Up) / HighlightDistance, 0.f, 1.f);

			return EGraspQueryResult::Highlight;
		}

		return EGraspQueryResult::None;
	}

	const float DistNormalized = Data->IsGraspDistance2D(Interactor) ? GraspPlanarDist(Location, InteractorLocation, Up) :
		FVector::Dist(Location, InteractorLocation);

	NormalizedDistance = FMath::Clamp(DistNormalized / Distance, 0.f, 1.f);

	return EGraspQueryResult::Interact;
}

bool UGraspStatics::CanInteractWithAngle(const AActor* Interactor, const UPrimitiveComponent* Graspable,
	float& NormalizedAngleDiff, int32 GraspDataIndex, EGraspUpMode UpMode, FVector CustomUp)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GraspStatics::CanInteractWithAngle);

	NormalizedAngleDiff = 0.f;

	// Validate the interactor
	if (!IsValid(Interactor))
	{
		return false;
	}

	// Validate the graspable
	if (!Graspable)
	{
		return false;
	}

	const UGraspData* Data = CastChecked<IGraspableComponent>(Graspable)->GetGraspData(GraspDataIndex);
	if (!Data)
	{
		return false;
	}

	FVector InteractorLocation = Interactor->GetActorLocation();
	GraspBaseFrame::GetInteractorLocationInBaseFrame(Interactor, Graspable, InteractorLocation);

	const FVector Location = Graspable->GetComponentLocation();
	const FVector Forward = GetGraspableForwardVector(Graspable, Data);
	const FVector Up = GetGraspUpVector(UpMode, Graspable, CustomUp);

	const float AuthNetToleranceAngleScalar = Data->GetAuthNetToleranceAngleScalar();
	const bool bApplyAuthScalar = Interactor->HasAuthority() && Interactor->GetNetMode() != NM_Standalone;

	const float BaseAngle = Data->GetMaxGraspAngle(Interactor);
	const float Angle = bApplyAuthScalar ? BaseAngle * AuthNetToleranceAngleScalar : BaseAngle;

	// Check if within angle
	if (!IsInteractableWithinAngle(Location, InteractorLocation, Forward, Angle, Up))
	{
		return false;
	}

	const float AngleNormalized = FMath::Clamp(
		GraspPlanarDist(Location, InteractorLocation, Up) / Angle, 0.f, 1.f);

	NormalizedAngleDiff = AngleNormalized;

	return true;
}

bool UGraspStatics::CanInteractWithHeight(const AActor* Interactor, const UPrimitiveComponent* Graspable,
	int32 GraspDataIndex, EGraspUpMode UpMode, FVector CustomUp)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GraspStatics::CanInteractWithHeight);

	// Validate the interactor
	if (!IsValid(Interactor))
	{
		return false;
	}

	// Validate the graspable
	if (!Graspable)
	{
		return false;
	}

	FVector InteractorLocation = Interactor->GetActorLocation();
	GraspBaseFrame::GetInteractorLocationInBaseFrame(Interactor, Graspable, InteractorLocation);

	const FVector Location = Graspable->GetComponentLocation();
	const UGraspData* Data = CastChecked<IGraspableComponent>(Graspable)->GetGraspData(GraspDataIndex);
	if (!Data)
	{
		return false;
	}

	const float AuthNetToleranceDistanceScalar = Data->GetAuthNetToleranceDistanceScalar();
	const bool bApplyAuthScalar = Interactor->HasAuthority() && Interactor->GetNetMode() != NM_Standalone;

	const float BaseHeightAbove = Data->GetMaxHeightAbove(Interactor);
	const float MaxHeightAbove = bApplyAuthScalar ? BaseHeightAbove * AuthNetToleranceDistanceScalar : BaseHeightAbove;

	const float BaseHeightBelow = Data->GetMaxHeightBelow(Interactor);
	const float MaxHeightBelow = bApplyAuthScalar ? BaseHeightBelow * AuthNetToleranceDistanceScalar : BaseHeightBelow;

	return IsInteractableWithinHeight(Location, InteractorLocation, MaxHeightAbove, MaxHeightBelow,
		GetGraspUpVector(UpMode, Graspable, CustomUp));
}

float UGraspStatics::GetNormalizedDistanceBetweenInteractAndHighlight(const UGraspData* GraspData,
	float NormalizedHighlightDistance)
{
	if (GraspData->MaxHighlightDistance <= 0.f)
	{
		return 0.f;
	}
	const float Divisor = GraspData->MaxGraspDistance / GraspData->MaxHighlightDistance;
	return UKismetMathLibrary::MapRangeClamped(NormalizedHighlightDistance, Divisor, 1.f, 0.f, 1.f);
}

FVector2D UGraspStatics::GetScreenPositionForGraspableComponent(const UPrimitiveComponent* GraspableComponent,
	APlayerController* PlayerController, bool& bSuccess, const UWidget* Widget)
{
	bSuccess = false;
	FVector2D ScreenPosition;
	if (!IsValid(PlayerController) || !IsValid(GraspableComponent))
	{
		return ScreenPosition;
	}

	// Get the screen position of the graspable component
	if (UGameplayStatics::ProjectWorldToScreen(PlayerController, GraspableComponent->GetComponentLocation(),
		ScreenPosition, true))
	{
		bSuccess = true;

		// Convert to viewport coordinates
		USlateBlueprintLibrary::ScreenToViewport(PlayerController, ScreenPosition, ScreenPosition);

		// Adjust for the widget half size to center the widget
		if (Widget)
		{
			const FVector2D DesiredSize = Widget->GetDesiredSize() * 0.5f;
			ScreenPosition -= DesiredSize;
		}
	}

	return ScreenPosition;
}

EGraspInteractionLocationResult UGraspStatics::GetInteractionLocationForGraspable(const FVector& InteractorLocation,
	const UPrimitiveComponent* GraspableComponent, FVector& OutLocation,
	int32 GraspDataIndex, float AngleAlpha, float DistanceAlpha, const AActor* Interactor,
	EGraspUpMode UpMode, FVector CustomUp)
{
	OutLocation = FVector::ZeroVector;

	if (!IsValid(GraspableComponent))
	{
		return EGraspInteractionLocationResult::Failed;
	}

	const UGraspData* GraspData = GetGraspData(GraspableComponent, GraspDataIndex);
	if (!GraspData)
	{
		return EGraspInteractionLocationResult::Failed;
	}

	const FVector GraspableLocation = GraspableComponent->GetComponentLocation();
	const FVector GraspableForward = GetGraspableForwardVector(GraspableComponent, GraspData);
	const FVector Up = GetGraspUpVector(UpMode, GraspableComponent, CustomUp);

	const float MaxGraspAngle = GraspData->GetMaxGraspAngle(Interactor);
	const float MaxGraspDistance = GraspData->GetMaxGraspDistance(Interactor);
	const bool bDistance2D = GraspData->IsGraspDistance2D(Interactor);

	const float CurrentDist = bDistance2D
		? GraspPlanarDist(InteractorLocation, GraspableLocation, Up)
		: FVector::Dist(InteractorLocation, GraspableLocation);

	const bool bInDistance = CurrentDist <= MaxGraspDistance;
	const bool bInAngle = (MaxGraspAngle >= 360.f) ||
		IsInteractableWithinAngle(GraspableLocation, InteractorLocation, GraspableForward, MaxGraspAngle, Up);

	// Case 1: Already valid
	if (bInDistance && bInAngle)
	{
		OutLocation = InteractorLocation;
		return EGraspInteractionLocationResult::AlreadyInRange;
	}

	AngleAlpha = FMath::Clamp(AngleAlpha, 0.f, 1.f);
	DistanceAlpha = FMath::Clamp(DistanceAlpha, 0.f, 1.f);

	const float HalfAngleDeg = MaxGraspAngle * 0.5f;
	const float MaxAllowedAngle = HalfAngleDeg * AngleAlpha;
	const float TargetDistance = MaxGraspDistance * DistanceAlpha;

	// Determine the target angle direction.
	// If in angle: use the interactor's current direction from the graspable (just fix distance).
	// If out of angle: clamp to the nearest edge of the valid cone.
	FVector TargetDir;

	if (bInAngle)
	{
		// Case 2: In angle, out of distance. Move straight toward the graspable along current direction.
		TargetDir = FVector::VectorPlaneProject(InteractorLocation - GraspableLocation, Up).GetSafeNormal();
	}
	else
	{
		// Cases 3 & 4: Out of angle. Compute which side of the cone is nearest and clamp.
		const FVector ToInteractor = FVector::VectorPlaneProject(InteractorLocation - GraspableLocation, Up).GetSafeNormal();
		const FVector Forward2D = FVector::VectorPlaneProject(GraspableForward, Up).GetSafeNormal();

		float CurrentAngleSign = 1.f;
		if (!ToInteractor.IsNearlyZero() && !Forward2D.IsNearlyZero())
		{
			CurrentAngleSign = FMath::Sign(FVector::CrossProduct(Forward2D, ToInteractor) | Up);
			if (FMath::IsNearlyZero(CurrentAngleSign))
			{
				CurrentAngleSign = 1.f;
			}
		}

		// Place at the nearest cone edge, pulled inward by AngleAlpha
		TargetDir = Forward2D.RotateAngleAxis(MaxAllowedAngle * CurrentAngleSign, Up);
	}

	// Determine the target distance.
	// If in distance: keep current distance (just fixing angle).
	// If out of distance: use the alpha-scaled max distance.
	const float FinalDist = bInDistance ? CurrentDist : TargetDistance;

	OutLocation = GraspableLocation + TargetDir * FinalDist;

	if (bDistance2D)
	{
		// Drop onto the plane through the graspable perpendicular to Up
		OutLocation += ((GraspableLocation - OutLocation) | Up) * Up;
	}

	return EGraspInteractionLocationResult::NeedsToMove;
}
