// Project:         Advanced Locomotion System V4 on C++
// Copyright:       Copyright (C) 2020 Doğa Can Yanıkoğlu
// License:         MIT License (http://www.opensource.org/licenses/mit-license.php)
// Source Code:     https://github.com/dyanikoglu/ALSV4_CPP
// Original Author: Doğa Can Yanıkoğlu
// Contributors:


#include "Library/ALSMathLibrary.h"
#include "ALS_Settings.h"
#include "Components/CapsuleComponent.h"
#include "Library/ALSCharacterStructLibrary.h"

FTransform UALSMathLibrary::MantleComponentLocalToWorld(const FALSComponentAndTransform& CompAndTransform)
{
	const FTransform& InverseTransform = CompAndTransform.Component->GetComponentToWorld().Inverse();
	const FVector Location = InverseTransform.InverseTransformPosition(CompAndTransform.Transform.GetLocation());
	const FQuat Quat = InverseTransform.InverseTransformRotation(CompAndTransform.Transform.GetRotation());
	const FVector Scale = InverseTransform.InverseTransformPosition(CompAndTransform.Transform.GetScale3D());
	return {Quat, Location, Scale};
}

TPair<float, float> UALSMathLibrary::FixDiagonalGamepadValues(const float X, const float Y)
{
	float ResultY = X * FMath::GetMappedRangeValueClamped(FVector2D(0.0f, 0.6f), FVector2D(1.0f, 1.2f), FMath::Abs(Y));
	ResultY = FMath::Clamp(ResultY, -1.0f, 1.0f);
	float ResultX = Y * FMath::GetMappedRangeValueClamped(FVector2D(0.0f, 0.6f), FVector2D(1.0f, 1.2f), FMath::Abs(X));
	ResultX = FMath::Clamp(ResultX, -1.0f, 1.0f);
	return TPair<float, float>(ResultY, ResultX);
}

FVector UALSMathLibrary::GetCapsuleBaseLocation(const float ZOffset, UCapsuleComponent* Capsule)
{
	return Capsule->GetComponentLocation() - Capsule->GetUpVector() * (Capsule->GetScaledCapsuleHalfHeight() + ZOffset);
}

FVector UALSMathLibrary::GetCapsuleLocationFromBase(FVector BaseLocation, const float ZOffset,
													UCapsuleComponent* Capsule)
{
	BaseLocation.Z += Capsule->GetScaledCapsuleHalfHeight() + ZOffset;
	return BaseLocation;
}

bool UALSMathLibrary::CapsuleHasRoomCheck(UCapsuleComponent* Capsule, const FVector TargetLocation,
										  const float HeightOffset, float RadiusOffset)
{
	// Perform a trace to see if the capsule has room to be at the target location.
	const float ZTarget = Capsule->GetScaledCapsuleHalfHeight_WithoutHemisphere() - RadiusOffset + HeightOffset;
	FVector TraceStart = TargetLocation;
	TraceStart.Z += ZTarget;
	FVector TraceEnd = TargetLocation;
	TraceEnd.Z -= ZTarget;
	const float Radius = Capsule->GetUnscaledCapsuleRadius() + RadiusOffset;

	const UWorld* World = Capsule->GetWorld();
	check(World);

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Capsule->GetOwner());

	FHitResult HitResult;
	World->SweepSingleByProfile(HitResult,
								TraceStart,
								TraceEnd,
								FQuat::Identity,
								UALS_Settings::Get()->ALS_Profile,
								FCollisionShape::MakeSphere(Radius),
								Params);

	return !(HitResult.bBlockingHit || HitResult.bStartPenetrating);
}

bool UALSMathLibrary::AngleInRange(const float Angle, const float MinAngle, const float MaxAngle, const float Buffer,
								   const bool IncreaseBuffer)
{
	if (IncreaseBuffer) { return Angle >= MinAngle - Buffer && Angle <= MaxAngle + Buffer; }
	return Angle >= MinAngle + Buffer && Angle <= MaxAngle - Buffer;
}

EALSMovementDirection UALSMathLibrary::CalculateQuadrant(const EALSMovementDirection Current, const float FRThreshold,
														 const float FLThreshold, const float BRThreshold,
														 const float BLThreshold, const float Buffer, const float Angle)
{
	// Take the input angle and determine its quadrant (direction). Use the current Movement Direction to increase or
	// decrease the buffers on the angle ranges for each quadrant.
	if (AngleInRange(Angle, FLThreshold, FRThreshold, Buffer,
	                 Current != EALSMovementDirection::Forward || Current != EALSMovementDirection::Backward))
	{
		return EALSMovementDirection::Forward;
	}
	if (AngleInRange(Angle, FRThreshold, BRThreshold, Buffer,
	                 Current != EALSMovementDirection::Right || Current != EALSMovementDirection::Left))
	{
		return EALSMovementDirection::Right;
	}
	if (AngleInRange(Angle, BLThreshold, FLThreshold, Buffer,
	                 Current != EALSMovementDirection::Right || Current != EALSMovementDirection::Left))
	{
		return EALSMovementDirection::Left;
	}
	return EALSMovementDirection::Backward;
}