// Copyright (c) Jared Taylor


#include "Targeting/GraspTargetSelection.h"

#include "GraspDeveloper.h"
#include "Components/CapsuleComponent.h"
#include "TargetingSystem/TargetingSubsystem.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "System/GraspVersioning.h"
#include "Targeting/GraspTargetingStatics.h"

#if UE_ENABLE_DEBUG_DRAWING
#if WITH_EDITORONLY_DATA
#include "Engine/Canvas.h"
#endif
#endif

DEFINE_LOG_CATEGORY_STATIC(LogGraspTargeting, Log, All);

#include UE_INLINE_GENERATED_CPP_BY_NAME(GraspTargetSelection)


namespace FGraspCVars
{
#if UE_ENABLE_DEBUG_DRAWING
	static bool bGraspSelectionDebug = false;
	FAutoConsoleVariableRef CVarGraspSelectionDebug(
		TEXT("p.Grasp.Selection.Debug"),
		bGraspSelectionDebug,
		TEXT("Optionally draw debug for Grasp AOE Selection Task.\n")
		TEXT("If true draw debug for Grasp AOE Selection Task"),
		ECVF_Default);
#endif
}

UGraspTargetSelection::UGraspTargetSelection(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	const TEnumAsByte<enum ECollisionChannel>& DefaultObjectType = GetDefault<UGraspDeveloper>()->GraspDefaultObjectType;
	const TEnumAsByte<EObjectTypeQuery> ObjectType = UCollisionProfile::Get()->ConvertToObjectType(DefaultObjectType);
	CollisionObjectTypes.Add(ObjectType);

	CollisionChannel = ECC_Visibility;
	bUseRelativeLocationOffset = true;
	bIgnoreSourceActor = true;
	bIgnoreInstigatorActor = false;
	bTraceComplex = false;

	LocationSource = EGraspTargetLocationSource::Actor;
	RotationSource = EGraspTargetRotationSource::Actor;
	FallbackRotationSources.Add(EGraspTargetRotationSource::Actor);

	MovementSelectionMode = EGraspMovementSelectionMode::Disabled;
	MovementSelectionAccelBias = 0.2f;  // Primarily from velocity

	ShapeType = EGraspTargetingShape::Capsule;
	
	HalfExtent = FVector(500.f, 380.f, 120.f);
	MaxHalfExtent = HalfExtent * 2.f;
	HalfHeight = 200.f;
	Radius = 200.f;
	MaxHalfHeight = 600.f;
	MaxRadius = 500.f;

	RadiusScalar = 12.f;
	MaxRadiusScalar = RadiusScalar.GetValue() * 2.f;
	HalfHeightScalar = 1.f;
	MaxHalfHeightScalar = 1.f;

	GraspAbilityRadius = 0.f;
	UpdateGraspAbilityRadius();
}

FVector UGraspTargetSelection::GetSourceLocation_Implementation(
	const FTargetingRequestHandle& TargetingHandle) const
{
	return UGraspTargetingStatics::GetSourceLocation(TargetingHandle, LocationSource);
}

FVector UGraspTargetSelection::GetSourceOffset_Implementation(
	const FTargetingRequestHandle& TargetingHandle) const
{
	return UGraspTargetingStatics::GetSourceOffset(TargetingHandle, LocationSource, DefaultSourceLocationOffset,
		bUseRelativeLocationOffset);
}

FQuat UGraspTargetSelection::GetSourceRotation_Implementation(
	const FTargetingRequestHandle& TargetingHandle) const
{
	const bool bUseFallback = RotationSource == EGraspTargetRotationSource::Velocity || RotationSource==EGraspTargetRotationSource::Acceleration;
	const TArray<EGraspTargetRotationSource> Fallbacks = bUseFallback ? FallbackRotationSources : TArray<EGraspTargetRotationSource>();
	return UGraspTargetingStatics::GetSourceRotation(TargetingHandle, RotationSource, Fallbacks);
}

FQuat UGraspTargetSelection::GetSourceRotationOffset_Implementation(
	const FTargetingRequestHandle& TargetingHandle) const
{
	return DefaultSourceRotationOffset.Quaternion();
}

void UGraspTargetSelection::UpdateGraspAbilityRadius()
{
	// Average the dimensions of the shape to get a radius
	switch (ShapeType)
	{
	case EGraspTargetingShape::Box:
	case EGraspTargetingShape::Cylinder:
		GraspAbilityRadius = 0.5f * (GetMaxHalfExtent().X + GetMaxHalfExtent().Y);  // Ignore Z (height)
		break;
	case EGraspTargetingShape::Sphere:
		GraspAbilityRadius = GetMaxRadius();
		break;
	case EGraspTargetingShape::Capsule:
		GraspAbilityRadius = 0.5f * (GetMaxRadius() + GetMaxHalfHeight());
		break;
	case EGraspTargetingShape::CharacterCapsule:
		GraspAbilityRadius = 0.5f * ((GetMaxRadius() * GetMaxRadiusScalar()) + (GetMaxHalfHeight() * GetMaxHalfHeightScalar()));
		break;
	}
}

void UGraspTargetSelection::PostLoad()
{
	Super::PostLoad();

	UpdateGraspAbilityRadius();
}

#if WITH_EDITOR
void UGraspTargetSelection::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	static const TArray<FName> ShapePropNames = {
		GET_MEMBER_NAME_CHECKED(ThisClass, HalfExtent),
		GET_MEMBER_NAME_CHECKED(ThisClass, Radius),
		GET_MEMBER_NAME_CHECKED(ThisClass, HalfHeight),
		GET_MEMBER_NAME_CHECKED(ThisClass, ShapeType)
	};

	// Average the dimensions of the shape to get a radius
	if (PropertyChangedEvent.ChangeType == EPropertyChangeType::ValueSet)
	{
		if (ShapePropNames.Contains(PropertyChangedEvent.GetMemberPropertyName()))
		{
			UpdateGraspAbilityRadius();
		}
	}
}
#endif

APawn* UGraspTargetSelection::GetPawnFromTargetingHandle(const FTargetingRequestHandle& TargetingHandle)
{ 
	if (TargetingHandle.IsValid())
	{
		if (const FTargetingSourceContext* SourceContext = FTargetingSourceContext::Find(TargetingHandle))
		{
			if (SourceContext->SourceActor)
			{
				if (const APawn* Pawn = Cast<APawn>(SourceContext->SourceActor))
				{
					return const_cast<APawn*>(Pawn);
				}
			}
		}
	}
	return nullptr;
}

bool UGraspTargetSelection::GetPawnCapsuleSize(const FTargetingRequestHandle& TargetingHandle,
	float& OutRadius, float& OutHalfHeight) const
{
	const APawn* Pawn = GetPawnFromTargetingHandle(TargetingHandle);
	if (!Pawn)
	{
		return false;
	}
	
	if (const ACharacter* Character = Cast<ACharacter>(Pawn))
	{
		if (const UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
		{
			OutHalfHeight = Capsule->GetScaledCapsuleHalfHeight();
			OutRadius = Capsule->GetScaledCapsuleRadius();
			return true;
		}
	}
	return false;
}

float UGraspTargetSelection::GetPawnMovementAlpha(const FTargetingRequestHandle& TargetingHandle) const
{
	if (MovementSelectionMode == EGraspMovementSelectionMode::Disabled)
	{
		return 0.f;
	}
	
	const APawn* Pawn = GetPawnFromTargetingHandle(TargetingHandle);
	if (!Pawn)
	{
		return 0.f;
	}
	
	if (const ACharacter* Character = Cast<ACharacter>(Pawn))
	{
		if (const UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
		{
			switch (MovementSelectionMode)
			{
			case EGraspMovementSelectionMode::Disabled:
				break;
			case EGraspMovementSelectionMode::Velocity:
				{
					const float Velocity = Movement->IsMovingOnGround() ? Movement->Velocity.Size() : Movement->Velocity.Size2D();
					const float MaxSpeed = Movement->GetMaxSpeed();
					return FMath::Clamp(Velocity / MaxSpeed, 0.f, 1.f);
				}
			case EGraspMovementSelectionMode::Acceleration:
				{
					const float Accel = Movement->GetCurrentAcceleration().Size2D();
					const float MaxAccel = Movement->GetMaxAcceleration();
					return FMath::Clamp(Accel / MaxAccel, 0.f, 1.f);
				}
			case EGraspMovementSelectionMode::VelocityAndAcceleration:
				{
					const float Velocity = Movement->IsMovingOnGround() ? Movement->Velocity.Size() : Movement->Velocity.Size2D();
					const float Accel = Movement->GetCurrentAcceleration().Size2D();
					
					const float MaxSpeed = Movement->GetMaxSpeed();
					const float MaxAccel = Movement->GetMaxAcceleration();
					
					const float AccelBias = FMath::Clamp(Accel / MaxAccel, 0.f, 1.f);
					const float VelBias = FMath::Clamp(Velocity / MaxSpeed, 0.f, 1.f);
					
					const float MovementSelectionVelBias = 1.f - MovementSelectionAccelBias;
					const float Alpha = 0.5f * ((AccelBias * MovementSelectionAccelBias) + (VelBias * MovementSelectionVelBias));
					return FMath::Clamp<float>(Alpha, 0.f, 1.f);
				}
			}
		}
	}
	return 0.f;
}

void UGraspTargetSelection::Execute(const FTargetingRequestHandle& TargetingHandle) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GraspTargetSelection::Execute);

	SetTaskAsyncState(TargetingHandle, ETargetingTaskAsyncState::Executing);

	// @note: There isn't Async Overlap support based on Primitive Component, so even if using async targeting, it will
	// run this task in "immediate" mode.
	if (IsAsyncTargetingRequest(TargetingHandle))
	{
		ExecuteAsyncTrace(TargetingHandle);
	}
	else
	{
		ExecuteImmediateTrace(TargetingHandle);
	}
}

void UGraspTargetSelection::ExecuteImmediateTrace(const FTargetingRequestHandle& TargetingHandle) const
{
#if UE_ENABLE_DEBUG_DRAWING
	ResetDebugString(TargetingHandle);
#endif // UE_ENABLE_DEBUG_DRAWING

	TRACE_CPUPROFILER_EVENT_SCOPE(GraspTargetSelection::ExecuteImmediateTrace);

	const UWorld* World = GetSourceContextWorld(TargetingHandle);
	if (World && TargetingHandle.IsValid())
	{
		const FVector SourceLocation = GetSourceLocation(TargetingHandle) + GetSourceOffset(TargetingHandle);
		const FQuat SourceRotation = (GetSourceRotation(TargetingHandle) * GetSourceRotationOffset(TargetingHandle)).GetNormalized();

		TArray<FOverlapResult> OverlapResults;
		const FCollisionShape CollisionShape = GetCollisionShape(TargetingHandle);
		FCollisionQueryParams OverlapParams(TEXT("UGraspTargetSelection_AOE"), SCENE_QUERY_STAT_ONLY(UGraspTargetSelection_AOE), false);
		InitCollisionParams(TargetingHandle, OverlapParams);

		if (CollisionObjectTypes.Num() > 0)
		{
			FCollisionObjectQueryParams ObjectParams;
			for (auto Iter = CollisionObjectTypes.CreateConstIterator(); Iter; ++Iter)
			{
				const ECollisionChannel& Channel = UCollisionProfile::Get()->ConvertToCollisionChannel(false, *Iter);
				ObjectParams.AddObjectTypesToQuery(Channel);
			}

			World->OverlapMultiByObjectType(OverlapResults, SourceLocation, SourceRotation, ObjectParams, CollisionShape, OverlapParams);
		}
		else if (CollisionProfileName.Name != TEXT("NoCollision"))
		{
			World->OverlapMultiByProfile(OverlapResults, SourceLocation, SourceRotation, CollisionProfileName.Name, CollisionShape, OverlapParams);
		}
		else
		{
			World->OverlapMultiByChannel(OverlapResults, SourceLocation, SourceRotation, CollisionChannel, CollisionShape, OverlapParams);
		}

		const int32 NumValidResults = ProcessOverlapResults(TargetingHandle, OverlapResults);
		
#if UE_ENABLE_DEBUG_DRAWING
		if (FGraspCVars::bGraspSelectionDebug)
		{
			const FColor& DebugColor = NumValidResults > 0 ? FColor::Red : FColor::Green;
			DebugDrawBoundingVolume(TargetingHandle, DebugColor);
		}
#endif
	}

	SetTaskAsyncState(TargetingHandle, ETargetingTaskAsyncState::Completed);
}

void UGraspTargetSelection::ExecuteAsyncTrace(const FTargetingRequestHandle& TargetingHandle) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GraspTargetSelection::ExecuteAsyncTrace);
	
	UWorld* World = GetSourceContextWorld(TargetingHandle);
	if (World && TargetingHandle.IsValid())
	{
		const FVector SourceLocation = GetSourceLocation(TargetingHandle) + GetSourceOffset(TargetingHandle);
		const FQuat SourceRotation = (GetSourceRotation(TargetingHandle) * GetSourceRotationOffset(TargetingHandle)).GetNormalized();

		const FCollisionShape CollisionShape = GetCollisionShape(TargetingHandle);
		FCollisionQueryParams OverlapParams(TEXT("UGraspTargetSelection_AOE"), SCENE_QUERY_STAT_ONLY(UGraspTargetSelection_AOE_Shape), false);
		InitCollisionParams(TargetingHandle, OverlapParams);

		const FOverlapDelegate Delegate = FOverlapDelegate::CreateUObject(this, &UGraspTargetSelection::HandleAsyncOverlapComplete, TargetingHandle);
		if (CollisionObjectTypes.Num() > 0)
		{
			FCollisionObjectQueryParams ObjectParams;
			for (auto Iter = CollisionObjectTypes.CreateConstIterator(); Iter; ++Iter)
			{
				const ECollisionChannel& Channel = UCollisionProfile::Get()->ConvertToCollisionChannel(false, *Iter);
				ObjectParams.AddObjectTypesToQuery(Channel);
			}

			World->AsyncOverlapByObjectType(SourceLocation, SourceRotation, ObjectParams, CollisionShape, OverlapParams, &Delegate);
		}
		else if (CollisionProfileName.Name != TEXT("NoCollision"))
		{
			World->AsyncOverlapByProfile(SourceLocation, SourceRotation, CollisionProfileName.Name, CollisionShape, OverlapParams, &Delegate);
		}
		else
		{
			World->AsyncOverlapByChannel(SourceLocation, SourceRotation, CollisionChannel, CollisionShape, OverlapParams, FCollisionResponseParams::DefaultResponseParam, &Delegate);
		}
	}
	else
	{
		SetTaskAsyncState(TargetingHandle, ETargetingTaskAsyncState::Completed);
	}
}

void UGraspTargetSelection::HandleAsyncOverlapComplete(const FTraceHandle& InTraceHandle,
	FOverlapDatum& InOverlapDatum, FTargetingRequestHandle TargetingHandle) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GraspTargetSelection::HandleAsyncOverlapComplete);
	
	if (TargetingHandle.IsValid())
	{
#if UE_ENABLE_DEBUG_DRAWING
		ResetDebugString(TargetingHandle);
#endif

		const int32 NumValidResults = ProcessOverlapResults(TargetingHandle, InOverlapDatum.OutOverlaps);
		
#if UE_ENABLE_DEBUG_DRAWING
		if (FGraspCVars::bGraspSelectionDebug)
		{
			const FColor& DebugColor = NumValidResults > 0 ? FColor::Red : FColor::Green;
			DebugDrawBoundingVolume(TargetingHandle, DebugColor, &InOverlapDatum);
		}
#endif
	}

	SetTaskAsyncState(TargetingHandle, ETargetingTaskAsyncState::Completed);
}

int32 UGraspTargetSelection::ProcessOverlapResults(const FTargetingRequestHandle& TargetingHandle,
	const TArray<FOverlapResult>& Overlaps) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GraspTargetSelection::ProcessOverlapResults);

#if WITH_EDITOR
	// During editor update this so we can modify properties during runtime
	UGraspTargetSelection* MutableThis = const_cast<UGraspTargetSelection*>(this);
	MutableThis->UpdateGraspAbilityRadius();
#endif
	
	// process the overlaps
	int32 NumValidResults = 0;
	if (Overlaps.Num() > 0)
	{
		FTargetingDefaultResultsSet& TargetingResults = FTargetingDefaultResultsSet::FindOrAdd(TargetingHandle);
		const FVector SourceLocation = GetSourceLocation(TargetingHandle) + GetSourceOffset(TargetingHandle);
		const FQuat SourceRotation = (GetSourceRotation(TargetingHandle) * GetSourceRotationOffset(TargetingHandle)).GetNormalized();

		for (const FOverlapResult& OverlapResult : Overlaps)
		{
			if (!OverlapResult.GetActor())
			{
				continue;
			}

			// cylinders use box overlaps, so a radius check is necessary to constrain it to the bounds of a cylinder
			if (ShapeType == EGraspTargetingShape::Cylinder)
			{
				const float RadiusSquared = (HalfExtent.X * HalfExtent.X);
				const float DistanceSquared = FVector::DistSquared2D(OverlapResult.GetActor()->GetActorLocation(), SourceLocation);
				if (DistanceSquared > RadiusSquared)
				{
					continue;
				}
			}

			bool bAddResult = true;
			for (const FTargetingDefaultResultData& ResultData : TargetingResults.TargetResults)
			{
				if (ResultData.HitResult.GetActor() == OverlapResult.GetActor())
				{
					bAddResult = false;
					break;
				}
			}

			if (bAddResult)
			{
				NumValidResults++;
				
				FTargetingDefaultResultData* ResultData = new(TargetingResults.TargetResults) FTargetingDefaultResultData();
				ResultData->HitResult.HitObjectHandle = OverlapResult.OverlapObjectHandle;
				ResultData->HitResult.Component = OverlapResult.GetComponent();
				ResultData->HitResult.ImpactPoint = OverlapResult.GetActor()->GetActorLocation();
				ResultData->HitResult.Location = OverlapResult.GetActor()->GetActorLocation();
				ResultData->HitResult.bBlockingHit = OverlapResult.bBlockingHit;
				ResultData->HitResult.TraceStart = SourceLocation;
				ResultData->HitResult.Item = OverlapResult.ItemIndex;

				// Store the normal based on where we are looking based on source rotation
				ResultData->HitResult.Normal = SourceRotation.Vector();

				// We need the normalized distance, which we calculate from GraspAbilityRadius
				ResultData->HitResult.Distance = GraspAbilityRadius;
			}
		}

#if UE_ENABLE_DEBUG_DRAWING
		BuildDebugString(TargetingHandle, TargetingResults.TargetResults);
#endif
	}

	return NumValidResults;
}

FCollisionShape UGraspTargetSelection::GetCollisionShape(const FTargetingRequestHandle& TargetingHandle) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GraspTargetSelection::GetCollisionShape);
	
	switch (ShapeType)
	{
	case EGraspTargetingShape::Box:
		{
			const FVector E = FMath::Lerp<FVector>(HalfExtent, MaxHalfExtent, GetPawnMovementAlpha(TargetingHandle));
			return FCollisionShape::MakeBox(E);
		}
	case EGraspTargetingShape::Cylinder:
		{
			const FVector E = FMath::Lerp<FVector>(HalfExtent, MaxHalfExtent, GetPawnMovementAlpha(TargetingHandle));
			return FCollisionShape::MakeBox(E);
		}
	case EGraspTargetingShape::Sphere:
		{
			const float R = FMath::Lerp<float>(Radius.GetValue(), MaxRadius.GetValue(), GetPawnMovementAlpha(TargetingHandle));
			return FCollisionShape::MakeSphere(R);
		}
	case EGraspTargetingShape::Capsule:
		{
			const float R = FMath::Lerp<float>(Radius.GetValue(), MaxRadius.GetValue(), GetPawnMovementAlpha(TargetingHandle));
			const float H = FMath::Lerp<float>(HalfHeight.GetValue(), MaxHalfHeight.GetValue(), GetPawnMovementAlpha(TargetingHandle));
			return FCollisionShape::MakeCapsule(R, H);
		}
	case EGraspTargetingShape::CharacterCapsule:
		{
			float CapsuleRadius = 0.f;
			float CapsuleHalfHeight = 0.f;
			if (GetPawnCapsuleSize(TargetingHandle, CapsuleRadius, CapsuleHalfHeight))
			{
				const float R = FMath::Lerp<float>(RadiusScalar.GetValue(), MaxRadiusScalar.GetValue(), GetPawnMovementAlpha(TargetingHandle));
				const float H = FMath::Lerp<float>(HalfHeightScalar.GetValue(), MaxHalfHeightScalar.GetValue(), GetPawnMovementAlpha(TargetingHandle));
				return FCollisionShape::MakeCapsule(CapsuleRadius * R, CapsuleHalfHeight * H);
			}
			else
			{
				const float R = FMath::Lerp<float>(Radius.GetValue(), MaxRadius.GetValue(), GetPawnMovementAlpha(TargetingHandle));
				const float H = FMath::Lerp<float>(HalfHeight.GetValue(), MaxHalfHeight.GetValue(), GetPawnMovementAlpha(TargetingHandle));
				return FCollisionShape::MakeCapsule(R, H);
			}
		}
	default: return {};
	}
}

void UGraspTargetSelection::InitCollisionParams(const FTargetingRequestHandle& TargetingHandle,
	FCollisionQueryParams& OutParams) const
{
	UGraspTargetingStatics::InitCollisionParams(TargetingHandle, OutParams, bIgnoreSourceActor,
		bIgnoreInstigatorActor, bTraceComplex);
}

void UGraspTargetSelection::DebugDrawBoundingVolume(const FTargetingRequestHandle& TargetingHandle,
	const FColor& Color, const FOverlapDatum* OverlapDatum) const
{
#if UE_ENABLE_DEBUG_DRAWING
	const UWorld* World = GetSourceContextWorld(TargetingHandle);
	const FVector SourceLocation = OverlapDatum ? OverlapDatum->Pos : GetSourceLocation(TargetingHandle) + GetSourceOffset(TargetingHandle);
	const FQuat SourceRotation = OverlapDatum ? OverlapDatum->Rot : (GetSourceRotation(TargetingHandle) * GetSourceRotationOffset(TargetingHandle)).GetNormalized();
	const FCollisionShape CollisionShape = GetCollisionShape(TargetingHandle);

	constexpr bool bPersistentLines = false;
#if UE_5_04_OR_LATER
	const float LifeTime = UTargetingSubsystem::GetOverrideTargetingLifeTime();
#else
	constexpr float LifeTime = 0.f;
#endif
	constexpr uint8 DepthPriority = 0;
	constexpr float Thickness = 2.0f;

	switch (ShapeType)
	{
	case EGraspTargetingShape::Box:
		DrawDebugBox(World, SourceLocation, CollisionShape.GetExtent(), SourceRotation,
			Color, bPersistentLines, LifeTime, DepthPriority, Thickness);
		break;
	case EGraspTargetingShape::Sphere:
		DrawDebugCapsule(World, SourceLocation, CollisionShape.GetSphereRadius(), CollisionShape.GetSphereRadius(), SourceRotation,
			Color, bPersistentLines, LifeTime, DepthPriority, Thickness);
		break;
	case EGraspTargetingShape::Capsule:
	case EGraspTargetingShape::CharacterCapsule:
		DrawDebugCapsule(World, SourceLocation, CollisionShape.GetCapsuleHalfHeight(), CollisionShape.GetCapsuleRadius(), SourceRotation,
			Color, bPersistentLines, LifeTime, DepthPriority, Thickness);
		break;
	case EGraspTargetingShape::Cylinder:
		{
			const FVector RotatedExtent = SourceRotation * CollisionShape.GetExtent();
			DrawDebugCylinder(World, SourceLocation - RotatedExtent, SourceLocation + RotatedExtent, CollisionShape.GetExtent().X, 32,
				Color, bPersistentLines, LifeTime, DepthPriority, Thickness);
			break;
		}
	}
#endif
}

#if UE_ENABLE_DEBUG_DRAWING
void UGraspTargetSelection::DrawDebug(UTargetingSubsystem* TargetingSubsystem, FTargetingDebugInfo& Info,
	const FTargetingRequestHandle& TargetingHandle, float XOffset, float YOffset, int32 MinTextRowsToAdvance) const
{
#if WITH_EDITORONLY_DATA
	if (FGraspCVars::bGraspSelectionDebug)
	{
		FTargetingDebugData& DebugData = FTargetingDebugData::FindOrAdd(TargetingHandle);
		const FString& ScratchPadString = DebugData.DebugScratchPadStrings.FindOrAdd(GetNameSafe(this));
		if (!ScratchPadString.IsEmpty())
		{
			if (Info.Canvas)
			{
				Info.Canvas->SetDrawColor(FColor::Yellow);
			}

			const FString TaskString = FString::Printf(TEXT("Results : %s"), *ScratchPadString);
			TargetingSubsystem->DebugLine(Info, TaskString, XOffset, YOffset, MinTextRowsToAdvance);
		}
	}
#endif
}

void UGraspTargetSelection::BuildDebugString(const FTargetingRequestHandle& TargetingHandle,
	const TArray<FTargetingDefaultResultData>& TargetResults) const
{
#if WITH_EDITORONLY_DATA
	if (FGraspCVars::bGraspSelectionDebug)
	{
		FTargetingDebugData& DebugData = FTargetingDebugData::FindOrAdd(TargetingHandle);
		FString& ScratchPadString = DebugData.DebugScratchPadStrings.FindOrAdd(GetNameSafe(this));

		for (const FTargetingDefaultResultData& TargetData : TargetResults)
		{
			if (const AActor* Target = TargetData.HitResult.GetActor())
			{
				if (ScratchPadString.IsEmpty())
				{
					ScratchPadString = FString::Printf(TEXT("%s"), *GetNameSafe(Target));
				}
				else
				{
					ScratchPadString += FString::Printf(TEXT(", %s"), *GetNameSafe(Target));
				}
			}
		}
	}
#endif
}

void UGraspTargetSelection::ResetDebugString(const FTargetingRequestHandle& TargetingHandle) const
{
#if WITH_EDITORONLY_DATA
	FTargetingDebugData& DebugData = FTargetingDebugData::FindOrAdd(TargetingHandle);
	FString& ScratchPadString = DebugData.DebugScratchPadStrings.FindOrAdd(GetNameSafe(this));
	ScratchPadString.Reset();
#endif
}
#endif
