// Copyright (c) Jared Taylor

#pragma once

#include "CoreMinimal.h"
#include "GraspTypes.h"
#include "Engine/DataAsset.h"
#include "GraspData.generated.h"

class UGameplayAbility;

/**
 * Data representing an interactable
 * This defines how we (the Pawn/Player/etc.) interact with the interactable actor,
 * as well as how the interactable actor behaves when interacted with.
 * Also includes parameters for adjusting the interaction distance and angle
 */
UCLASS(BlueprintType, Blueprintable)
class GRASP_API UGraspData : public UDataAsset
{
	GENERATED_BODY()

public:
	UGraspData()
		: GraspAbility(nullptr)
		, FocusRequirement(EGraspFocusMode::None)
		, MaxGraspAngle(360.f)
		, MaxGraspDistance(200.f)
		, MaxHighlightDistance(400.f)
		, MaxHeightAbove(30.f)
		, MaxHeightBelow(30.f)
		, NormalizedGrantAbilityDistance(0.7f)
		, AuthNetToleranceAnglePct(10.f)
		, AuthNetToleranceDistancePct(10.f)
		, bGrantAbilityDistance2D(false)
		, bGraspDistance2D(false)
	{}
	
	/**
	 * The ability granted to the interactor,
	 * Behaviour for the interactor when interacting with this data's owner
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Grasp)
	TSubclassOf<UGameplayAbility> GraspAbility;

	/**
	 * An optional property you can utilize to determine if the ability should be activable, or continue to be active, based on whether focus is lost
	 * Pair this with Vigil or any other focus system
	 * Not implemented by default but common enough that it should be here
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Grasp)
	EGraspFocusMode FocusRequirement;

	/**
	 * Angle where we can interact with the interactable
	 * Based on the Forward Vector of the GraspableComponent holding this Data
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Grasp, meta=(UIMin="0", ClampMin="0", UIMax="360", ClampMax="360", Delta="1", ForceUnits="Degrees"))
	float MaxGraspAngle;

	/** Distance when we can interact with the interactable */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Grasp, meta=(UIMin="0", ClampMin="0", Delta="1", ForceUnits="cm"))
	float MaxGraspDistance;

	/**
	 * Distance when we can focus on the interactable
	 * Typically used for UI/Visualization purposes to demonstrate we are nearing the interactable range
	 * Set to 0.0 to disable
	 * Not implemented by default but common enough that it should be here
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Grasp, meta=(UIMin="0", ClampMin="0", Delta="1", ForceUnits="cm"))
	float MaxHighlightDistance;

	/**
	 * Maximum height the interactor can be above the interactable
	 * Stops us from interacting with things that are below us
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Grasp, meta=(UIMin="0", ClampMin="0", Delta="1", ForceUnits="cm"))
	float MaxHeightAbove;

	/**
	 * Maximum height the interactor can be below the interactable
	 * Stops us from interacting with things that are above us
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Grasp, meta=(UIMin="0", ClampMin="0", Delta="1", ForceUnits="cm"))
	float MaxHeightBelow;
	
	/**
	 * Normalized Distance when we grant the interact ability
	 * Distance is normalized between 0 and the max pre-scan range utilized by Grasp's Targeting System
	 * We generally want to grant abilities when they're closer than the distance where we remove them (at max pre-scan range)
	 * This stops us constantly adding and removing abilities which has performance overhead
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Grasp, meta=(UIMin="0", ClampMin="0", UIMax="1", ClampMax="1", Delta="0.05", ForceUnits="Percent"))
	float NormalizedGrantAbilityDistance;

	/**
	 * Angle will effectively be increased by this amount on server authority for angle checks
	 * This allows potentially minor discrepancies in distance checks to be ignored for ability activation
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category=Grasp, meta=(UIMin="0", ClampMin="0", UIMax="100", Delta="0.5", ForceUnits="Percent"))
	float AuthNetToleranceAnglePct;
	
	/**
	 * Distance will effectively be increased by this amount on server authority for distance checks
	 * This allows potentially minor discrepancies in distance checks to be ignored for ability activation
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category=Grasp, meta=(UIMin="0", ClampMin="0", UIMax="100", Delta="0.5", ForceUnits="Percent"))
	float AuthNetToleranceDistancePct;

	/**
	 * Get the angle tolerance scalar for server authority
	 * @return 1.f + (AuthNetToleranceAnglePct / 100.f)
	 */
	float GetAuthNetToleranceAngleScalar() const { return 1.f + (AuthNetToleranceAnglePct / 100.f); }

	/**
	 * Get the distance tolerance scalar for server authority
	 * @return 1.f + (AuthNetToleranceDistancePct / 100.f)
	 */
	float GetAuthNetToleranceDistanceScalar() const { return 1.f + (AuthNetToleranceDistancePct / 100.f); }
	
	/**
	 * Use 2D distance checks for granting the ability
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Grasp)
	bool bGrantAbilityDistance2D;
	
	/**
	 * Use 2D distance checks for testing MaxGraspDistance and MaxHighlightDistance
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Grasp)
	bool bGraspDistance2D;
	
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
