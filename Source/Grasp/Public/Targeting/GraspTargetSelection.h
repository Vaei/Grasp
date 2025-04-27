// Copyright (c) Jared Taylor

#pragma once

#include "CoreMinimal.h"
#include "GraspTargetingTypes.h"
#include "Tasks/TargetingSelectionTask_AOE.h"
#include "GraspTargetSelection.generated.h"

/**
 * Extend targeting for interaction selection
 * Adds location and rotation sources
 */
UCLASS(Blueprintable, DisplayName="Grasp Target Selection")
class GRASP_API UGraspTargetSelection : public UTargetingTask
{
	GENERATED_BODY()

protected:
	/** The collision channel to use for the overlap check (as long as Collision Profile Name is not set) */
	UPROPERTY(EditAnywhere, Category="Grasp Selection")
	TEnumAsByte<ECollisionChannel> CollisionChannel;
	
	/** The collision profile name to use for the overlap check */
	UPROPERTY(EditAnywhere, Category="Grasp Selection")
	FCollisionProfileName CollisionProfileName;

	/** The object types to use for the overlap check */
	UPROPERTY(EditAnywhere, Category="Grasp Selection")
	TArray<TEnumAsByte<EObjectTypeQuery>> CollisionObjectTypes;

	/** Location to trace from */
	UPROPERTY(EditAnywhere, Category="Grasp Selection")
	EGraspTargetLocationSource LocationSource;

	/** Rotation to trace from */
	UPROPERTY(EditAnywhere, Category="Grasp Selection")
	EGraspTargetRotationSource RotationSource;

	/** Fallback Rotation to trace from if RotationSource could not get a valid direction (e.g. velocity, but we have no velocity) */
	UPROPERTY(EditAnywhere, Category="Grasp Selection", meta=(EditCondition="RotationSource==EGraspTargetRotationSource::Velocity||RotationSource==EGraspTargetRotationSource::Acceleration", EditConditionHides))
	TArray<EGraspTargetRotationSource> FallbackRotationSources;

	/** The default source location offset used by GetSourceOffset */
	UPROPERTY(EditAnywhere, Category="Grasp Selection")
	FVector DefaultSourceLocationOffset = FVector::ZeroVector;

	/** Should we offset based on world or relative Source object transform? */
	UPROPERTY(EditAnywhere, Category="Grasp Selection")
	uint8 bUseRelativeLocationOffset : 1;

	/** The default source rotation offset used by GetSourceOffset */
	UPROPERTY(EditAnywhere, Category="Grasp Selection")
	FRotator DefaultSourceRotationOffset = FRotator::ZeroRotator;

protected:
	/** When enabled, the trace will be performed against complex collision. */
	UPROPERTY(EditAnywhere, Category="Grasp Selection")
	uint8 bTraceComplex : 1 = false;
	
protected:
	/** Indicates the trace should ignore the source actor */
	UPROPERTY(EditAnywhere, Category="Grasp Selection")
	uint8 bIgnoreSourceActor : 1;

	/** Indicates the trace should ignore the source actor */
	UPROPERTY(EditAnywhere, Category="Grasp Selection")
	uint8 bIgnoreInstigatorActor : 1;
	
protected:
	/** The shape type to use for the AOE */
	UPROPERTY(EditAnywhere, Category="Grasp Selection Shape")
	EGraspTargetingShape ShapeType;

	/**
	 * Modify the shape based on our character movement properties
	 * To support Pawns you will need to subclass this and override GetPawnMovementScalar()
	 */
	UPROPERTY(EditAnywhere, Category="Grasp Selection Shape")
	EGraspMovementSelectionMode MovementSelectionMode;

	/**
	 * Change the bias between acceleration and velocity
	 * At 1.0 the shape will be based on the acceleration of the character
	 * At 0.0 the shape will be based on the velocity of the character
	 * At 0.5 the shape will be based on the average of the two
	 */
	UPROPERTY(EditAnywhere, Category="Grasp Selection Shape", meta=(UIMin="0", ClampMin="0", UIMax="1", ClampMax="1", Delta="0.05", ForceUnits="Percent", EditCondition="MovementSelectionMode!=EGraspMovementSelectionMode::Disabled", EditConditionHides))
	float MovementSelectionAccelBias;
	
	/** The half extent to use for box and cylinder */
	UPROPERTY(EditAnywhere, Category="Grasp Selection Shape", meta=(EditCondition="ShapeType==EGraspTargetingShape::Box||ShapeType==EGraspTargetingShape::Cylinder", EditConditionHides))
	FVector HalfExtent;
	
	/** The maximum half extent to use for box and cylinder from MovementSelectionMode */
	UPROPERTY(EditAnywhere, Category="Grasp Selection Shape", meta=(EditCondition="MovementSelectionMode!=EGraspMovementSelectionMode::Disabled&&(ShapeType==EGraspTargetingShape::Box||ShapeType==EGraspTargetingShape::Cylinder)", EditConditionHides))
	FVector MaxHalfExtent;
	
	/** The radius to use for sphere and capsule overlaps */
	UPROPERTY(EditAnywhere, Category="Grasp Selection Shape", meta=(EditCondition="ShapeType==EGraspTargetingShape::Sphere||ShapeType==EGraspTargetingShape::Capsule", EditConditionHides))
	FScalableFloat Radius;

	/** The half height to use for capsule overlap checks */
	UPROPERTY(EditAnywhere, Category="Grasp Selection Shape", meta=(EditCondition="ShapeType==EGraspTargetingShape::Capsule", EditConditionHides))
	FScalableFloat HalfHeight;

	/** The radius to use for sphere and capsule overlaps */
	UPROPERTY(EditAnywhere, Category="Grasp Selection Shape", meta=(EditCondition="MovementSelectionMode!=EGraspMovementSelectionMode::Disabled&&(ShapeType==EGraspTargetingShape::Sphere||ShapeType==EGraspTargetingShape::Capsule)", EditConditionHides))
	FScalableFloat MaxRadius;

	/** The half height to use for capsule overlap checks */
	UPROPERTY(EditAnywhere, Category="Grasp Selection Shape", meta=(EditCondition="MovementSelectionMode!=EGraspMovementSelectionMode::Disabled&&(ShapeType==EGraspTargetingShape::Capsule)", EditConditionHides))
	FScalableFloat MaxHalfHeight;
	
	/** The radius to use for sphere and capsule overlaps */
	UPROPERTY(EditAnywhere, Category="Grasp Selection Shape", meta=(ForceUnits="x", EditCondition="ShapeType==EGraspTargetingShape::CharacterCapsule", EditConditionHides))
	FScalableFloat RadiusScalar;

	/** The half height to use for capsule overlap checks */
	UPROPERTY(EditAnywhere, Category="Grasp Selection Shape", meta=(ForceUnits="x", EditCondition="ShapeType==EGraspTargetingShape::CharacterCapsule", EditConditionHides))
	FScalableFloat HalfHeightScalar;

	/** The radius to use for sphere and capsule overlaps */
	UPROPERTY(EditAnywhere, Category="Grasp Selection Shape", meta=(ForceUnits="x", EditCondition="MovementSelectionMode!=EGraspMovementSelectionMode::Disabled&&(ShapeType==EGraspTargetingShape::CharacterCapsule)", EditConditionHides))
	FScalableFloat MaxRadiusScalar;

	/** The half height to use for capsule overlap checks */
	UPROPERTY(EditAnywhere, Category="Grasp Selection Shape", meta=(ForceUnits="x", EditCondition="MovementSelectionMode!=EGraspMovementSelectionMode::Disabled&&(ShapeType==EGraspTargetingShape::CharacterCapsule)", EditConditionHides))
	FScalableFloat MaxHalfHeightScalar;

	float GetMaxRadius() const { return MovementSelectionMode != EGraspMovementSelectionMode::Disabled ? MaxRadius.GetValue() : Radius.GetValue(); }
	float GetMaxHalfHeight() const { return MovementSelectionMode != EGraspMovementSelectionMode::Disabled ? MaxHalfHeight.GetValue() : HalfHeight.GetValue(); }
	float GetMaxRadiusScalar() const { return MovementSelectionMode != EGraspMovementSelectionMode::Disabled ? MaxRadiusScalar.GetValue() : RadiusScalar.GetValue(); }
	float GetMaxHalfHeightScalar() const { return MovementSelectionMode != EGraspMovementSelectionMode::Disabled ? MaxHalfHeightScalar.GetValue() : HalfHeightScalar.GetValue(); }
	FVector GetMaxHalfExtent() const { return MovementSelectionMode != EGraspMovementSelectionMode::Disabled ? MaxHalfExtent : HalfExtent; }
	
	/**
	 * Radius used by Grasp for granting abilities
	 * Calculated based on the shape dimensions
	 * @see UGraspData::NormalizedGrantAbilityDistance
	 */
	UPROPERTY(VisibleAnywhere, Category="Grasp Selection Shape")
	float GraspAbilityRadius;

public:
	UGraspTargetSelection(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	/** Native Event to get the source location for the AOE */
	UFUNCTION(BlueprintNativeEvent, Category="Grasp Selection")
	FVector GetSourceLocation(const FTargetingRequestHandle& TargetingHandle) const;

	/** Native Event to get a source location offset for the AOE */
	UFUNCTION(BlueprintNativeEvent, Category="Grasp Selection")
	FVector GetSourceOffset(const FTargetingRequestHandle& TargetingHandle) const;

	/** Native event to get the source rotation for the AOE  */
	UFUNCTION(BlueprintNativeEvent, Category="Grasp Selection")
	FQuat GetSourceRotation(const FTargetingRequestHandle& TargetingHandle) const;

	/** Native event to get the source rotation for the AOE  */
	UFUNCTION(BlueprintNativeEvent, Category="Grasp Selection")
	FQuat GetSourceRotationOffset(const FTargetingRequestHandle& TargetingHandle) const;

public:
	void UpdateGraspAbilityRadius();

	virtual void PostLoad() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	static APawn* GetPawnFromTargetingHandle(const FTargetingRequestHandle& TargetingHandle);
	virtual bool GetPawnCapsuleSize(const FTargetingRequestHandle& TargetingHandle, float& OutRadius, float& OutHalfHeight) const;
	virtual float GetPawnMovementAlpha(const FTargetingRequestHandle& TargetingHandle) const;
	
	/** Evaluation function called by derived classes to process the targeting request */
	virtual void Execute(const FTargetingRequestHandle& TargetingHandle) const override;

protected:
	/** Method to process the trace task immediately */
	void ExecuteImmediateTrace(const FTargetingRequestHandle& TargetingHandle) const;

	/** Method to process the trace task asynchronously */
	void ExecuteAsyncTrace(const FTargetingRequestHandle& TargetingHandle) const;

	/** Callback for an async overlap */
	void HandleAsyncOverlapComplete(const FTraceHandle& InTraceHandle, FOverlapDatum& InOverlapDatum,
		FTargetingRequestHandle TargetingHandle) const;

	/**
	 * Method to take the overlap results and store them in the targeting result data
	 * @return Num valid results
	 */
	int32 ProcessOverlapResults(const FTargetingRequestHandle& TargetingHandle, const TArray<FOverlapResult>& Overlaps) const;
	
protected:
	/** Helper method to build the Collision Shape */
	FCollisionShape GetCollisionShape(const FTargetingRequestHandle& TargetingHandle) const;
	
	/** Setup CollisionQueryParams for the AOE */
	void InitCollisionParams(const FTargetingRequestHandle& TargetingHandle, FCollisionQueryParams& OutParams) const;
	
public:
	/** Debug draws the outlines of the set shape type. */
	void DebugDrawBoundingVolume(const FTargetingRequestHandle& TargetingHandle, const FColor& Color,
		const FOverlapDatum* OverlapDatum = nullptr) const;

#if UE_ENABLE_DEBUG_DRAWING
protected:
	virtual void DrawDebug(UTargetingSubsystem* TargetingSubsystem, FTargetingDebugInfo& Info,
		const FTargetingRequestHandle& TargetingHandle, float XOffset, float YOffset, int32 MinTextRowsToAdvance) const override;
	void BuildDebugString(const FTargetingRequestHandle& TargetingHandle, const TArray<FTargetingDefaultResultData>& TargetResults) const;
	void ResetDebugString(const FTargetingRequestHandle& TargetingHandle) const;
#endif
};
