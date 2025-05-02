// Copyright (c) Jared Taylor


#include "Tasks/RootMotionSource_GraspFaceTowards.h"

#include "GameFramework/Character.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RootMotionSource_GraspFaceTowards)


FRootMotionSource_GraspFaceTowards::FRootMotionSource_GraspFaceTowards()
{
	// Disable Partial End Tick
	// Otherwise we end up with very inconsistent velocities on the last frame.
	// This ensures that the ending velocity is maintained and consistent.
	Settings.SetFlag(ERootMotionSourceSettingsFlags::DisablePartialEndTick);
}

FRootMotionSource* FRootMotionSource_GraspFaceTowards::Clone() const
{
	FRootMotionSource_GraspFaceTowards* CopyPtr = new FRootMotionSource_GraspFaceTowards(*this);
	return CopyPtr;
}

bool FRootMotionSource_GraspFaceTowards::Matches(const FRootMotionSource* Other) const
{
	if (!FRootMotionSource::Matches(Other))
	{
		return false;
	}

	// We can cast safely here since in FRootMotionSource::Matches() we ensured ScriptStruct equality
	const FRootMotionSource_GraspFaceTowards* OtherCast = static_cast<const FRootMotionSource_GraspFaceTowards*>(Other);

	return StartDirection.Equals(OtherCast->StartDirection, THRESH_NORMALS_ARE_PARALLEL) &&
		TargetDirection.Equals(OtherCast->TargetDirection, THRESH_NORMALS_ARE_PARALLEL);
}

void FRootMotionSource_GraspFaceTowards::PrepareRootMotion(float SimulationTime, float MovementTickTime,
	const ACharacter& Character, const UCharacterMovementComponent& MoveComponent)
{
	RootMotionParams.Clear();

	if (Duration > UE_SMALL_NUMBER)
	{
		const float MoveFraction = FMath::Clamp(GetTime() / Duration, 0.f, 1.f);

		const FQuat StartQuat = StartDirection.ToOrientationQuat();
		const FQuat TargetQuat = TargetDirection.ToOrientationQuat();

		FQuat CurrentQuat = FQuat::Slerp(StartQuat, TargetQuat, MoveFraction);

		CurrentQuat = Character.GetActorTransform().InverseTransformRotation(CurrentQuat);
		
		const bool bFinished = FMath::IsNearlyEqual(MoveFraction, 1.f) || CurrentQuat.Equals(TargetQuat, KINDA_SMALL_NUMBER);
		if (bFinished)
		{
			CurrentQuat = TargetQuat;
		}
	
		FTransform NewTransform;
		NewTransform.SetRotation(CurrentQuat);
	
#if ROOT_MOTION_DEBUG
		if (RootMotionSourceDebug::CVarDebugRootMotionSources.GetValueOnAnyThread() == 1)
		{
			const FString AdjustedDebugString = FString::Printf(TEXT("FRootMotionSource_facetowards::PrepareRootMotion NewTransform(%s) Duration(%f)"),
				*NewTransform.GetRotation().Vector().ToCompactString(), Duration);
			RootMotionSourceDebug::PrintOnScreen(Character, AdjustedDebugString);
		}
#endif
		
		if (bFinished)
		{
			Status.SetFlag(ERootMotionSourceStatusFlags::Finished);
		}
		
		RootMotionParams.Set(NewTransform);
	}

	SetTime(GetTime() + SimulationTime);
}

bool FRootMotionSource_GraspFaceTowards::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	if (!FRootMotionSource::NetSerialize(Ar, Map, bOutSuccess))
	{
		return false;
	}

	Ar << StartDirection;
	Ar << TargetDirection;

	bOutSuccess = true;
	return true;
}

FString FRootMotionSource_GraspFaceTowards::ToSimpleString() const
{
	return FString::Printf(TEXT("[ID:%u]FRootMotionSource_GraspFaceTowards %s"), LocalID, *InstanceName.GetPlainNameString());
}
