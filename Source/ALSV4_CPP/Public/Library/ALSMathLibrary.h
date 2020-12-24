// Project:         Advanced Locomotion System V4 on C++
// Copyright:       Copyright (C) 2020 Doğa Can Yanıkoğlu
// License:         MIT License (http://www.opensource.org/licenses/mit-license.php)
// Source Code:     https://github.com/dyanikoglu/ALSV4_CPP
// Original Author: Doğa Can Yanıkoğlu
// Contributors:    


#pragma once

#include "ALSCharacterEnumLibrary.h"
#include "CoreMinimal.h"
#include "ALSMathLibrary.generated.h"

struct FALSComponentAndTransform;
class UCapsuleComponent;

/**
 * Math library functions for ALS
 */
UCLASS()
class ALSV4_CPP_API UALSMathLibrary final : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "ALS Math")
	static FTransform MantleComponentLocalToWorld(const FALSComponentAndTransform& CompAndTransform);

	static TPair<float, float> FixDiagonalGamepadValues(float Y, float X);

	UFUNCTION(BlueprintCallable, Category = "ALS Math")
	static FTransform TransfromSub(const FTransform& T1, const FTransform& T2)
	{
		return FTransform(T1.GetRotation().Rotator() - T2.GetRotation().Rotator(),
						  T1.GetLocation() - T2.GetLocation(),
						  T1.GetScale3D() - T2.GetScale3D());
	}

	UFUNCTION(BlueprintCallable, Category = "ALS Math")
	static FTransform TransfromAdd(const FTransform& T1, const FTransform& T2)
	{
		return FTransform(T1.GetRotation().Rotator() + T2.GetRotation().Rotator(),
						  T1.GetLocation() + T2.GetLocation(),
						  T1.GetScale3D() + T2.GetScale3D());
	}

	UFUNCTION(BlueprintCallable, Category = "ALS Math")
	static FVector GetCapsuleBaseLocation(float ZOffset, UCapsuleComponent* Capsule);

	UFUNCTION(BlueprintCallable, Category = "ALS Math")
	static FVector GetCapsuleLocationFromBase(FVector BaseLocation, float ZOffset, UCapsuleComponent* Capsule);

	UFUNCTION(BlueprintCallable, Category = "ALS Math")
	static bool CapsuleHasRoomCheck(UCapsuleComponent* Capsule, FVector TargetLocation, float HeightOffset,
									float RadiusOffset);

	UFUNCTION(BlueprintCallable, Category = "ALS Math")
	static bool AngleInRange(float Angle, float MinAngle, float MaxAngle, float Buffer, bool IncreaseBuffer);

	UFUNCTION(BlueprintCallable, Category = "ALS Math")
	static EALSMovementDirection CalculateQuadrant(EALSMovementDirection Current, float FRThreshold, float FLThreshold,
												   float BRThreshold, float BLThreshold, float Buffer, float Angle);
};
