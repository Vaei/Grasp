// Copyright (c) Jared Taylor


#include "GraspClaim.h"

#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GraspClaim)

namespace
{
	int32 FindEntryIndex(const FGraspClaimArray& Array, const AActor* Owner, FName ComponentName)
	{
		for (int32 i = 0; i < Array.Entries.Num(); ++i)
		{
			const FGraspClaimEntry& Entry = Array.Entries[i];
			if (Entry.Owner == Owner && Entry.ComponentName == ComponentName)
			{
				return i;
			}
		}
		return INDEX_NONE;
	}
}

bool UGraspClaimStatics::TryClaimSlot(FGraspClaimArray& Array, AActor* Owner, FName ComponentName, uint8 SlotIndex)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UGraspClaimStatics::TryClaimSlot);

	if (!Owner || ComponentName.IsNone() || SlotIndex >= 8)
	{
		return false;
	}

	const uint8 Bit = static_cast<uint8>(1u << SlotIndex);

	const int32 ExistingIndex = FindEntryIndex(Array, Owner, ComponentName);
	if (ExistingIndex != INDEX_NONE)
	{
		FGraspClaimEntry& Entry = Array.Entries[ExistingIndex];
		if ((Entry.SlotMask & Bit) != 0)
		{
			return false;
		}
		Entry.SlotMask |= Bit;
		Array.MarkItemDirty(Entry);
		return true;
	}

	FGraspClaimEntry NewEntry;
	NewEntry.Owner = Owner;
	NewEntry.ComponentName = ComponentName;
	NewEntry.SlotMask = Bit;
	FGraspClaimEntry& Added = Array.Entries.Add_GetRef(NewEntry);
	Array.MarkItemDirty(Added);
	return true;
}

void UGraspClaimStatics::ReleaseSlot(FGraspClaimArray& Array, AActor* Owner, FName ComponentName, uint8 SlotIndex)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UGraspClaimStatics::ReleaseSlot);

	if (!Owner || ComponentName.IsNone() || SlotIndex >= 8)
	{
		return;
	}

	const uint8 Bit = static_cast<uint8>(1u << SlotIndex);
	const int32 Index = FindEntryIndex(Array, Owner, ComponentName);
	if (Index == INDEX_NONE)
	{
		return;
	}

	FGraspClaimEntry& Entry = Array.Entries[Index];
	if ((Entry.SlotMask & Bit) == 0)
	{
		return;
	}

	Entry.SlotMask &= static_cast<uint8>(~Bit);
	if (Entry.SlotMask == 0)
	{
		Array.Entries.RemoveAt(Index);
		Array.MarkArrayDirty();
	}
	else
	{
		Array.MarkItemDirty(Entry);
	}
}

bool UGraspClaimStatics::IsSlotClaimed(const FGraspClaimArray& Array, const AActor* Owner, FName ComponentName, uint8 SlotIndex)
{
	if (!Owner || ComponentName.IsNone() || SlotIndex >= 8)
	{
		return false;
	}
	const uint8 Bit = static_cast<uint8>(1u << SlotIndex);
	const int32 Index = FindEntryIndex(Array, Owner, ComponentName);
	if (Index == INDEX_NONE)
	{
		return false;
	}
	return (Array.Entries[Index].SlotMask & Bit) != 0;
}

UPrimitiveComponent* UGraspClaimStatics::ResolveGraspable(const FGraspClaimEntry& Entry)
{
	if (!Entry.Owner || Entry.ComponentName.IsNone())
	{
		return nullptr;
	}

	if (UObject* AsDefault = Entry.Owner->GetDefaultSubobjectByName(Entry.ComponentName))
	{
		return Cast<UPrimitiveComponent>(AsDefault);
	}

	TArray<UActorComponent*> Components;
	Entry.Owner->GetComponents(UPrimitiveComponent::StaticClass(), Components);
	for (UActorComponent* Comp : Components)
	{
		if (Comp && Comp->GetFName() == Entry.ComponentName)
		{
			return Cast<UPrimitiveComponent>(Comp);
		}
	}
	return nullptr;
}

void UGraspClaimStatics::RemoveOwnerEntries(FGraspClaimArray& Array, const AActor* Owner)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UGraspClaimStatics::RemoveOwnerEntries);

	if (!Owner)
	{
		return;
	}
	bool bChanged = false;
	for (int32 i = Array.Entries.Num() - 1; i >= 0; --i)
	{
		if (Array.Entries[i].Owner == Owner)
		{
			Array.Entries.RemoveAt(i);
			bChanged = true;
		}
	}
	if (bChanged)
	{
		Array.MarkArrayDirty();
	}
}

void UGraspClaimStatics::RemoveStaleEntries(FGraspClaimArray& Array)
{
	bool bChanged = false;
	for (int32 i = Array.Entries.Num() - 1; i >= 0; --i)
	{
		if (!Array.Entries[i].Owner)
		{
			Array.Entries.RemoveAt(i);
			bChanged = true;
		}
	}
	if (bChanged)
	{
		Array.MarkArrayDirty();
	}
}

bool UGraspClaimStatics::TryClaimSlotOnHost(AActor* Host, AActor* Owner, FName ComponentName, uint8 SlotIndex)
{
	IGraspClaimHost* ClaimHost = Cast<IGraspClaimHost>(Host);
	if (!ClaimHost)
	{
		return false;
	}
	FGraspClaimArray* Array = ClaimHost->GetGraspClaimArray();
	if (!Array)
	{
		return false;
	}
	return TryClaimSlot(*Array, Owner, ComponentName, SlotIndex);
}

void UGraspClaimStatics::ReleaseSlotOnHost(AActor* Host, AActor* Owner, FName ComponentName, uint8 SlotIndex)
{
	IGraspClaimHost* ClaimHost = Cast<IGraspClaimHost>(Host);
	if (!ClaimHost)
	{
		return;
	}
	FGraspClaimArray* Array = ClaimHost->GetGraspClaimArray();
	if (!Array)
	{
		return;
	}
	ReleaseSlot(*Array, Owner, ComponentName, SlotIndex);
}

bool UGraspClaimStatics::IsSlotClaimedOnHost(AActor* Host, const AActor* Owner, FName ComponentName, uint8 SlotIndex)
{
	const IGraspClaimHost* ClaimHost = Cast<IGraspClaimHost>(Host);
	if (!ClaimHost)
	{
		return false;
	}
	const FGraspClaimArray* Array = ClaimHost->GetGraspClaimArray();
	if (!Array)
	{
		return false;
	}
	return IsSlotClaimed(*Array, Owner, ComponentName, SlotIndex);
}
