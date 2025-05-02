// Copyright (c) Jared Taylor


#include "GraspableVisualizer.h"

#include "GraspableComponent.h"
#include "GraspData.h"
#include "Materials/MaterialRenderProxy.h"


namespace GraspCVars
{
	static bool bShouldVisualizeGrasp = true;
	static FAutoConsoleVariableRef CVarShouldVisualizeGrasp(
		TEXT("p.Grasp.Visualize"),
		bShouldVisualizeGrasp,
		TEXT("If true will draw visuals for Graspable/GraspData to visualize angle, distance, height, etc."),
		ECVF_Default);
}


void FGraspableVisualizer::DrawVisualization(const UActorComponent* InComponent, const FSceneView* View,
	FPrimitiveDrawInterface* PDI)
{
	if (!GraspCVars::bShouldVisualizeGrasp)
	{
		return;
	}
	
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

	// Retrieve the transform properties
	FTransform Transform = Component->GetComponentTransform();
	Transform.SetRotation(FRotator(0.f, Transform.Rotator().Yaw, 0.f).Quaternion());
	const FVector& BaseLocation = Component->GetComponentLocation();
	const FVector& Forward = Transform.GetUnitAxis(EAxis::X);
	const FVector& Right = Transform.GetUnitAxis(EAxis::Y);
	const FVector& Up = Transform.GetUnitAxis(EAxis::Z);
	const float Radius = Component->Bounds.SphereRadius * 1.2f;

	// Retrieve the Graspable Interface and Data
	const IGraspableComponent* Graspable = CastChecked<IGraspableComponent>(Component);
	const UGraspData* Data = Graspable->GetGraspData();

	// Colors for Drawing
	const FColor Color = FColor::Green;
	const FColor RemColor = FColor::Black;
	const FColor ErrorColor = FColor::Red;

	// If no Data is found draw a red error circle/disc
	if (!Data)
	{
		// Draw outline circle
		DrawCircle(PDI, BaseLocation, Forward, Right, ErrorColor, Radius, 16, SDPG_Foreground, 1.f);

		// Draw inner disc
		DrawDisc(PDI, BaseLocation, Forward, Right, ErrorColor, Radius, 16, Proxy, SDPG_Foreground);
		return;
	}

	// Draw from above the BaseLocation based on the MaxHeightAbove
	const FVector Location = BaseLocation + Up * Data->MaxHeightAbove;

	// 360 to 180
	const float Angle = FRotator::NormalizeAxis(-(Data->MaxGraspAngle * 0.5f));

	// More segments based on the angle
	static constexpr int32 MaxSections = 32;
	const int32 Sections = FMath::Max(MaxSections, FMath::CeilToInt(Angle / 180.f * MaxSections));

	// Draw Outer and Below if applicable
	const bool bDrawOuter = !FMath::IsNearlyZero(Data->MaxHighlightDistance) && !FMath::IsNearlyEqual(Data->MaxHighlightDistance, Data->MaxGraspDistance);
	const bool bDrawBelow = !FMath::IsNearlyZero(Data->MaxHeightAbove) || !FMath::IsNearlyZero(Data->MaxHeightBelow);

	// Determine the location below the BaseLocation based on the MaxHeightBelow
	const FVector LocationBelow = BaseLocation - Up * Data->MaxHeightBelow;

	// Distance based on whether we highlight or not
	const float Distance = bDrawOuter ? Data->MaxHighlightDistance : Data->MaxGraspDistance;
	
	// Inner Arc representing the angle and grasp distance
	DrawArc(PDI, Location, Forward, Right, -Angle, Angle, Data->MaxGraspDistance, Sections, Color, SDPG_Foreground);
	DrawCircle(PDI, Location, Forward, Right, RemColor, Data->MaxGraspDistance, Sections, SDPG_World);
	if (bDrawBelow)
	{
		DrawCircle(PDI, LocationBelow, Forward, Right, RemColor, Data->MaxGraspDistance, Sections, SDPG_World);
	}
	
	// Outer Arc representing the angle and highlight distance
	if (bDrawOuter)
	{
		DrawArc(PDI, Location, Forward, Right, -Angle, Angle, Data->MaxHighlightDistance, Sections, Color, SDPG_Foreground);
		DrawCircle(PDI, Location, Forward, Right, RemColor, Data->MaxHighlightDistance, Sections, SDPG_World, 1.f);
		if (bDrawBelow)
		{
			DrawCircle(PDI, LocationBelow, Forward, Right, RemColor, Data->MaxHighlightDistance, Sections, SDPG_World, 1.f);
		}
	}

	// Draw a circle below the BaseLocation if applicable
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

		// Draw a line from the origin to the inner arc showing the angle we can interact at
		{
			const float A = AngleRadians + (Sections-1) * -DeltaAngle;

			FVector Dir = Forward * FMath::Cos(A) + Right * FMath::Sin(A);
			FVector Start = Location;
			FVector End = Location + Dir * Distance;

			PDI->DrawLine(Start, End, Color, SDPG_World, 1.f);
		}
		// Draw a line from the origin to the center of the arc showing the angle we can interact at
		{
			FVector Start = Location;
			FVector End = Location + Forward * Distance;

			PDI->DrawLine(Start, End, Color, SDPG_World, 1.f);
		}
		// Draw a line from the origin to the outer arc showing the angle we can interact at
		{
			const float A = -AngleRadians + (Sections-1) * DeltaAngle;

			FVector Dir = Forward * FMath::Cos(A) + Right * FMath::Sin(A);
			FVector Start = Location;
			FVector End = Location + Dir * Distance;

			PDI->DrawLine(Start, End, Color, SDPG_World, 1.f);
		}

		// Draw lines representing the angle we can interact with (shading)
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
