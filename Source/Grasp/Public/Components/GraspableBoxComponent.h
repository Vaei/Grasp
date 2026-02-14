// Copyright (c) Jared Taylor

#pragma once

#include "CoreMinimal.h"
#include "GraspableComponent.h"
#include "GraspDeveloper.h"
#include "Components/BoxComponent.h"
#include "GraspStatics.h"
#include "GraspableBoxComponent.generated.h"

class UGraspData;

/**
 * This component is placed on the interactable actor
 * It defines a point from which interaction can occur
 * And provides a suitable target for focusing systems e.g. Vigil
 */
UCLASS(Blueprintable, BlueprintType, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class GRASP_API UGraspableBoxComponent : public UBoxComponent, public IGraspableComponent
{
	GENERATED_BODY()

public:
	/* IGraspable */
	virtual const UGraspData* GetGraspData(int32 Index = 0) const override final
	{
		return GraspDataEntries.IsValidIndex(Index) ? GraspDataEntries[Index].Get() : nullptr;
	}
	
	virtual const TArray<TObjectPtr<UGraspData>>* GetGraspDataEntries() const override { return &GraspDataEntries; }
	
	virtual int32 GetNumGraspData() const override final { return GraspDataEntries.Num(); }
#if WITH_EDITORONLY_DATA
	virtual int32 GetGraspVisualizationIndex() const override final { return GraspVisualizationIndex; }
#endif
	virtual bool IsGraspableDead() const override
	{
		if (K2_IsGraspableDead()) {	return true; }
		return false;
	}
	/* ~IGraspable */

#if WITH_EDITORONLY_DATA
	virtual void PostLoad() override
	{
		Super::PostLoad();
		GRASP_MIGRATE_DEPRECATED_DATA()
	}
#endif

public:
#if WITH_EDITORONLY_DATA
	/** @deprecated Use GraspDataEntries instead */
	UPROPERTY()
	TObjectPtr<UGraspData> GraspData_DEPRECATED;
#endif

	/** Interaction data entries. Each entry can grant a different ability with its own range/angle parameters */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Grasp, meta=(DisplayName="Grasp Data"))
	TArray<TObjectPtr<UGraspData>> GraspDataEntries;


#if WITH_EDITORONLY_DATA
	/** Index of GraspData entry to visualize in editor (-1 = all, 0 = first, etc.) */
	UPROPERTY(EditAnywhere, Category="Grasp|Debug", meta=(DisplayName="Visualize Data Index", ClampMin="-1"))
	int32 GraspVisualizationIndex = 0;
#endif

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
	UFUNCTION(BlueprintImplementableEvent, Category=Grasp, meta=(DisplayName="Is Graspable Dead"))
	bool K2_IsGraspableDead() const;

public:
	UGraspableBoxComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get())
		: Super(ObjectInitializer)
	{
		// This component exists solely for the Targeting System to find it, nothing else
		PrimaryComponentTick.bCanEverTick = false;
		PrimaryComponentTick.bStartWithTickEnabled = false;
		PrimaryComponentTick.bAllowTickOnDedicatedServer = false;
		SetIsReplicatedByDefault(false);

		APPLY_GRASP_DEFAULT_COLLISION_SETTINGS(BodyInstance, GetName());

		SetGenerateOverlapEvents(false);
		CanCharacterStepUpOn = ECB_No;
		bCanEverAffectNavigation = false;
		bAutoActivate = false;

		SetHiddenInGame(true);

		LineThickness = 1.f;
		ShapeColor = FColor::Magenta;
	}
};
