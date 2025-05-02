// Copyright (c) Jared Taylor

#pragma once

#include "CoreMinimal.h"
#include "GraspableComponent.h"
#include "Components/BoxComponent.h"
#include "GraspDeveloper.h"
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
	virtual const UGraspData* GetGraspData() const override final { return GraspData; }
	virtual bool IsGraspableDead() const override
	{
		if (K2_IsGraspableDead()) {	return true; }
		return false;
	}
	/* ~IGraspable */
	
public:
	/**
	 * The GraspData for this component.
	 * This defines how we (the Pawn/Player/etc.) interact,
	 * as well as how the interactable behaves when interacted with.
	 *
	 * Includes parameters for adjusting the interaction distance, angle, height, etc.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Grasp)
	TObjectPtr<UGraspData> GraspData;

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
		
		BodyInstance.SetResponseToAllChannels(ECR_Ignore);
		BodyInstance.SetObjectType(ECC_WorldDynamic);
		BodyInstance.SetCollisionEnabled(ECollisionEnabled::QueryOnly);

		SetGenerateOverlapEvents(false);
		CanCharacterStepUpOn = ECB_No;
		bCanEverAffectNavigation = false;
		bAutoActivate = false;

		SetHiddenInGame(true);

		LineThickness = 1.f;
		ShapeColor = FColor::Magenta;
	}

#if WITH_EDITOR
	virtual void OnComponentCreated() override
	{
		Super::OnComponentCreated();

		if (IsTemplate())
		{
			if (const UGraspDeveloper* GraspDeveloper = GetDefault<UGraspDeveloper>())
			{
				SetCollisionObjectType(GraspDeveloper->GraspDefaultObjectType);
			}
		}
	}
#endif
};
