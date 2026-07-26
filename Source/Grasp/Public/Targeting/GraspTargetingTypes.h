// Copyright (c) Jared Taylor

#pragma once

#include "CoreMinimal.h"
#include "GraspTargetingTypes.generated.h"


UENUM(BlueprintType)
enum class EGraspTargetingShape : uint8
{
	Box,
	Cylinder,
	Sphere,
	Capsule,
	CharacterCapsule,
};

UENUM(BlueprintType)
enum class EGraspTargetLocationSource : uint8
{
	Actor,
	ViewLocation,
	Camera,
};

UENUM(BlueprintType)
enum class EGraspTargetRotationSource : uint8
{
	Actor,
	ControlRotation,
	ViewRotation,
	Velocity,
	Acceleration,
};

/**
 * Which up direction Grasp selection constraints are measured against
 */
UENUM(BlueprintType)
enum class EGraspSelectionUpMode : uint8
{
	WorldUp				UMETA(ToolTip="Use the world up vector (+Z)"),
	SourceRotationUp	UMETA(ToolTip="Use the up axis of the resolved source rotation (e.g. the view or camera rotation)"),
	SourceActorUp		UMETA(ToolTip="Use the source actor's up vector"),
	CustomUp			UMETA(ToolTip="Use the supplied custom up vector, e.g. the source's gravity up"),
};

UENUM(BlueprintType)
enum class EGraspMovementSelectionMode : uint8
{
	Disabled					UMETA(ToolTip="Don't factor Velocity or Acceleration"),
	Velocity					UMETA(ToolTip="Factor Velocity into the selection process"),
	Acceleration				UMETA(ToolTip="Factor Acceleration into the selection process"),
	VelocityAndAcceleration		UMETA(ToolTip="Factor both Velocity and Acceleration into the selection process"),
};
