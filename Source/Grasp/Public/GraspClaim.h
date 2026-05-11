// Copyright (c) Jared Taylor

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "UObject/Interface.h"
#include "GraspClaim.generated.h"

class UPrimitiveComponent;

/**
 * Per-graspable claim row. Identifies the graspable by (Owner, ComponentName) since graspable
 * UPrimitiveComponents are not network-addressable. SlotMask packs claim state
 * for up to 8 slots: bit N set means slot N is claimed.
 */
USTRUCT(BlueprintType)
struct GRASP_API FGraspClaimEntry : public FFastArraySerializerItem
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<AActor> Owner = nullptr;

	UPROPERTY()
	FName ComponentName = NAME_None;

	UPROPERTY()
	uint8 SlotMask = 0;
};

/**
 * Replicated host-level claim state. Hosted directly by any actor that wants to own claim state
 * (typically via IGraspClaimHost) as a UPROPERTY(Replicated). Sized for actor relevancy: a
 * player only receives entries for hosts they're currently relevant to.
 *
 * Mutations route through UGraspClaimStatics so the same logic is reused across host actor types
 * without inheritance. Mutations are server-authoritative and must be performed only on actors
 * with HasAuthority(); the array delta-replicates to relevant clients automatically.
 */
USTRUCT(BlueprintType)
struct GRASP_API FGraspClaimArray : public FFastArraySerializer
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FGraspClaimEntry> Entries;

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FGraspClaimEntry, FGraspClaimArray>(Entries, DeltaParms, *this);
	}
};

template<>
struct TStructOpsTypeTraits<FGraspClaimArray> : public TStructOpsTypeTraitsBase2<FGraspClaimArray>
{
	enum { WithNetDeltaSerializer = true };
};

/**
 * Type-erased getter that lets ability/AI code resolve the claim array on whatever actor is
 * acting as the claim host. Implementers return a pointer into their own UPROPERTY storage.
 */
UINTERFACE(MinimalAPI, meta=(CannotImplementInterfaceInBlueprint))
class UGraspClaimHost : public UInterface
{
	GENERATED_BODY()
};

class GRASP_API IGraspClaimHost
{
	GENERATED_BODY()

public:
	virtual FGraspClaimArray* GetGraspClaimArray() = 0;
	virtual const FGraspClaimArray* GetGraspClaimArray() const = 0;
};

/**
 * Free-function operations on FGraspClaimArray. Mutations are server-only; the host actor is
 * responsible for the HasAuthority() gate at its call site. The helpers handle FastArraySerializer
 * dirty-marking internally.
 */
UCLASS()
class GRASP_API UGraspClaimStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Try to claim a slot. Returns false if the slot is already claimed or inputs are invalid.
	 * Server only. Creates an entry for (Owner, ComponentName) if none exists.
	 */
	static bool TryClaimSlot(FGraspClaimArray& Array, AActor* Owner, FName ComponentName, uint8 SlotIndex);

	/**
	 * Release a previously claimed slot. No-op if not claimed. Drops the entry if its mask
	 * becomes zero so the array stays compact. Server only.
	 */
	static void ReleaseSlot(FGraspClaimArray& Array, AActor* Owner, FName ComponentName, uint8 SlotIndex);

	/** Query whether a slot is currently claimed. Safe on any side. */
	static bool IsSlotClaimed(const FGraspClaimArray& Array, const AActor* Owner, FName ComponentName, uint8 SlotIndex);

	/**
	 * Resolve the entry's graspable component on the local peer. Tries default-subobject lookup
	 * first then falls back to a name scan over the actor's components for runtime-spawned ones.
	 * Returns null if Owner is gone or no matching component exists. Safe on any side.
	 */
	static UPrimitiveComponent* ResolveGraspable(const FGraspClaimEntry& Entry);

	/**
	 * Drop every entry belonging to the given Owner. Host actors should call this from a bound
	 * Owner OnDestroyed handler so claims do not outlive their graspable's actor. Server only.
	 */
	static void RemoveOwnerEntries(FGraspClaimArray& Array, const AActor* Owner);

	/**
	 * Drop every entry whose Owner is null. Sweep helper; host actors can call periodically as
	 * a safety net for missed destroy notifications. Server only.
	 */
	static void RemoveStaleEntries(FGraspClaimArray& Array);

	// Host-scoped BP wrappers. Look up the array via IGraspClaimHost and forward.

	UFUNCTION(BlueprintCallable, Category="Grasp|Claim", meta=(DefaultToSelf="Host"))
	static bool TryClaimSlotOnHost(AActor* Host, AActor* Owner, FName ComponentName, uint8 SlotIndex);

	UFUNCTION(BlueprintCallable, Category="Grasp|Claim", meta=(DefaultToSelf="Host"))
	static void ReleaseSlotOnHost(AActor* Host, AActor* Owner, FName ComponentName, uint8 SlotIndex);

	UFUNCTION(BlueprintPure, Category="Grasp|Claim", meta=(DefaultToSelf="Host"))
	static bool IsSlotClaimedOnHost(AActor* Host, const AActor* Owner, FName ComponentName, uint8 SlotIndex);
};
