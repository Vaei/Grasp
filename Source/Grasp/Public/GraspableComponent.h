// Copyright (c) Jared Taylor

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "GraspableComponent.generated.h"

struct FGameplayAbilityActorInfo;
struct FGameplayAbilityTargetData;
class UGraspData;

UINTERFACE()
class GRASP_API UGraspableComponent : public UInterface
{
	GENERATED_BODY()
};

/**
 * Inheritance band-aid, we want multiple derived UPrimitiveComponents to be able to implement this interface
 * This isn't intended for use outside the Grasp plugin
 * Implementation is assumed and never tested
 */
class GRASP_API IGraspableComponent
{
	GENERATED_BODY()

public:
	/**
	 * The GraspData for this component at the given index.
	 * This defines how we (the Pawn/Player/etc.) interact,
	 * as well as how the interactable behaves when interacted with.
	 *
	 * Includes parameters for adjusting the interaction distance, angle, height, etc.
	 */
	virtual const UGraspData* GetGraspData(int32 Index = 0) const PURE_VIRTUAL(, return nullptr;);
	
	/** @return All GraspData entries on this component. */
	virtual const TArray<TObjectPtr<UGraspData>>* GetGraspDataEntries() const PURE_VIRTUAL(, return nullptr;);

	/** @return The number of GraspData entries on this component */
	virtual int32 GetNumGraspData() const { return 0; }

	/** @return The visualization index for the editor visualizer (-1 = all, 0+ = specific entry) */
#if WITH_EDITORONLY_DATA
	virtual int32 GetGraspVisualizationIndex() const { return 0; }
#endif

	/**
	 * Optional additional target data that will be passed to the ability when the graspable is interacted with.
	 * @return The optional target data for this graspable.
	 */
	virtual TArray<FGameplayAbilityTargetData*> GatherOptionalGraspTargetData(const FGameplayAbilityActorInfo* ActorInfo) const
	{
		return {};
	}

	/**
	 * Dead graspables have their abilities removed from the Pawn that they were granted to.
	 *
	 * If the graspable becomes available again in the future and is interacted with immediately after,
	 * before the ability is re-granted -- there will be de-sync.
	 *
	 * You do not need to check IsPendingKillPending() or IsTornOff() on the owner, this is done for you.
	 *
	 * @return True if this graspable is no longer available, e.g. a Barrel that is exploding, a Pawn who is dying.
	 */
	virtual bool IsGraspableDead() const { return false; }
};

// Migrate deprecated single GraspData to GraspDataEntries array
#if WITH_EDITORONLY_DATA
#define GRASP_MIGRATE_DEPRECATED_DATA() \
if (GraspData_DEPRECATED) \
{ \
	if (GraspDataEntries.Num() == 0) \
	{ \
		GraspDataEntries.Add(GraspData_DEPRECATED); \
	} \
	GraspData_DEPRECATED = nullptr; \
}
#endif

// Inheritance band-aid...
#define APPLY_GRASP_DEFAULT_COLLISION_SETTINGS(BodyInstance, GetNameFunc) \
if (const UGraspDeveloper* GraspDeveloper = GetDefault<UGraspDeveloper>()) \
{ \
	switch (GraspDeveloper->GraspDefaultCollisionMode) \
	{ \
	case EGraspDefaultCollisionMode::Profile: \
		if (BodyInstance.GetCollisionProfileName() != GraspDeveloper->GraspDefaultCollisionProfile.Name) \
		{ \
			BodyInstance.SetCollisionProfileName(GraspDeveloper->GraspDefaultCollisionProfile.Name); \
		} \
		break; \
	case EGraspDefaultCollisionMode::ObjectType: \
		if (BodyInstance.GetObjectType() != GraspDeveloper->GraspDefaultObjectType) \
		{ \
			BodyInstance.SetObjectType(GraspDeveloper->GraspDefaultObjectType); \
			if (GraspDeveloper->bSetDefaultOverlapChannel && \
				BodyInstance.GetResponseToChannel(GraspDeveloper->GraspDefaultOverlapChannel) != ECR_Overlap) \
			{ \
				BodyInstance.SetResponseToChannel(GraspDeveloper->GraspDefaultOverlapChannel, ECR_Overlap); \
			} \
		} \
		else if (GraspDeveloper->bSetDefaultOverlapChannel && \
				BodyInstance.GetResponseToChannel(GraspDeveloper->GraspDefaultOverlapChannel) != ECR_Overlap) \
		{ \
			BodyInstance.SetResponseToChannel(GraspDeveloper->GraspDefaultOverlapChannel, ECR_Overlap); \
		} \
		break; \
	case EGraspDefaultCollisionMode::Disabled: \
		break; \
	} \
}