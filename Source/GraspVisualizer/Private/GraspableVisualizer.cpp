// Copyright (c) Jared Taylor


#include "GraspableVisualizer.h"

#include "Graspable.h"
#include "GraspData.h"
#include "Materials/MaterialRenderProxy.h"


void FGraspableVisualizer::DrawVisualization(const UActorComponent* InComponent, const FSceneView* View,
	FPrimitiveDrawInterface* PDI)
{
	const UPrimitiveComponent* Component = InComponent ? Cast<const UPrimitiveComponent>(InComponent) : nullptr;
	if (!Component || !IsValid(Component->GetOwner()))
	{
		return;
	}

	const FColoredMaterialRenderProxy* Proxy = new FColoredMaterialRenderProxy(
		GEngine->WireframeMaterial ? GEditor->ConstraintLimitMaterialPrismatic->GetRenderProxy() : nullptr,
		FLinearColor(0.0f, 0.0f, 0.0f, 0.0f)
	);

	// Can't draw without a Proxy
	if (!Proxy)
	{
		return;
	}

	FTransform Transform = Component->GetComponentTransform();
	Transform.SetRotation(FRotator(0.f, Transform.Rotator().Yaw, 0.f).Quaternion());
	const FVector& BaseLocation = Component->GetComponentLocation();
	const FVector& Forward = Transform.GetUnitAxis(EAxis::X);
	const FVector& Right = Transform.GetUnitAxis(EAxis::Y);
	const FVector& Up = Transform.GetUnitAxis(EAxis::Z);
	const float Radius = Component->Bounds.SphereRadius * 1.2f;

	const IGraspable* Graspable = CastChecked<IGraspable>(Component);
	const UGraspData* Data = Graspable->GetGraspData();

	const FColor Color = FColor::Green;
	const FColor RemColor = FColor::Black;
	const FColor ErrorColor = FColor::Red;
	
	if (!Data)
	{
		// Draw outline circle
		DrawCircle(PDI, BaseLocation, Forward, Right, ErrorColor, Radius, 16, SDPG_Foreground, 1.f);

		// Draw inner disc
		DrawDisc(PDI, BaseLocation, Forward, Right, ErrorColor, Radius, 16, Proxy, SDPG_Foreground);
		return;
	}

	const FVector Location = BaseLocation + Up * Data->MaxHeightAbove;

	// 360 to 180
	const float Angle = FRotator::NormalizeAxis(-(Data->MaxGraspAngle * 0.5f));

	// More segments based on the angle
	static constexpr int32 MaxSections = 32;
	const int32 Sections = FMath::Max(MaxSections, FMath::CeilToInt(Angle / 180.f * MaxSections));

	const bool bDrawOuter = !FMath::IsNearlyZero(Data->MaxHighlightDistance) && !FMath::IsNearlyEqual(Data->MaxHighlightDistance, Data->MaxGraspDistance);
	const bool bDrawBelow = !FMath::IsNearlyZero(Data->MaxHeightAbove) || !FMath::IsNearlyZero(Data->MaxHeightBelow);
	const FVector LocationBelow = BaseLocation - Up * Data->MaxHeightBelow;
	
	const float Distance = bDrawOuter ? Data->MaxHighlightDistance : Data->MaxGraspDistance;
	
	// Inner Arc representing the angle and grasp distance
	DrawArc(PDI, Location, Forward, Right, -Angle, Angle, Data->MaxGraspDistance, Sections, Color, SDPG_Foreground);
	DrawCircle(PDI, Location, Forward, Right, RemColor, Data->MaxGraspDistance, Sections, SDPG_World);
	if (bDrawBelow)
	{
		DrawCircle(PDI, LocationBelow, Forward, Right, RemColor, Data->MaxGraspDistance, Sections, SDPG_World);
	}
	
	if (bDrawOuter)
	{
		// Outer Arc representing the angle and highlight distance
		DrawArc(PDI, Location, Forward, Right, -Angle, Angle, Data->MaxHighlightDistance, Sections, Color, SDPG_Foreground);
		DrawCircle(PDI, Location, Forward, Right, RemColor, Data->MaxHighlightDistance, Sections, SDPG_World, 1.f);
		if (bDrawBelow)
		{
			DrawCircle(PDI, LocationBelow, Forward, Right, RemColor, Data->MaxHighlightDistance, Sections, SDPG_World, 1.f);
		}
	}

	// Shading
	// DrawDisc(PDI, Location, Forward, Right, FColor::White, Distance, Sections, Proxy, SDPG_World);

	// Draw above visualizer

	if (bDrawBelow)
	{
		// DrawDisc(PDI, LocationBelow, Forward, Right, FColor::White, Distance, Sections, Proxy, SDPG_World);
		DrawCircle(PDI, LocationBelow, Forward, Right, RemColor, Distance, Sections, SDPG_World, 1.f);
	}

	// Draw lines to shade the arc
	if (!FMath::IsNearlyZero(Angle))
	{
		const float AngleRadians = FMath::DegreesToRadians(Angle);
		const float DeltaAngle = (AngleRadians * 2.f) / (Sections - 1);

		for (int32 i = 0; i < Sections; ++i)
		{
			const float A = -AngleRadians + i * DeltaAngle;

			// 2D polar to 3D vector using Forward and Right basis
			FVector Dir = Forward * FMath::Cos(A) + Right * FMath::Sin(A);
			FVector Start = bDrawOuter ? Location + Dir * Data->MaxGraspDistance : Location;
			FVector End = Location + Dir * Distance;

			PDI->DrawLine(Start, End, Color, SDPG_World, 1.f);

			if (bDrawBelow) 
			{
				const FVector StartBelow = bDrawOuter ? LocationBelow + Dir * Data->MaxGraspDistance : Location;
				FVector EndBelow = LocationBelow + Dir * Distance;
				PDI->DrawLine(Start, StartBelow, RemColor, SDPG_World, 1.f);
				PDI->DrawLine(End, EndBelow, RemColor, SDPG_World, 1.f);
			}
		}

		// Do the same for the part that we didn't shade, i.e. the angle we can't interact at
		if (!FMath::IsNearlyEqual(Angle, 180.f)) 
		{
			const int32 RemSections = FMath::Max(MaxSections, FMath::CeilToInt(180.f / Angle * MaxSections));

			const float RemAngleRadians = FMath::DegreesToRadians(FRotator::NormalizeAxis(180.f - Angle));
			const float RemDeltaAngle = (RemAngleRadians * 2.f) / (RemSections - 1);

			for (int32 i = 0; i < RemSections; ++i)
			{
				const float A = -RemAngleRadians + i * RemDeltaAngle;

				// 2D polar to 3D vector using Forward and Right basis
				FVector Dir = -Forward * FMath::Cos(A) + Right * FMath::Sin(A);
				FVector Start = bDrawOuter ? Location + Dir * Data->MaxHighlightDistance : Location;
				FVector End = Location + Dir * Data->MaxGraspDistance;

				PDI->DrawLine(Start, End, RemColor, SDPG_World, 1.f);

				if (bDrawBelow) 
				{
					const FVector StartBelow = bDrawOuter ? LocationBelow + Dir * Data->MaxGraspDistance : Location;
					FVector EndBelow = LocationBelow + Dir * Distance;
					PDI->DrawLine(Start, EndBelow, RemColor, SDPG_World, 1.f);
					PDI->DrawLine(End, StartBelow, RemColor, SDPG_World, 1.f);
				}
			}
		}
	}
}
