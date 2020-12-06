// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Character/ALSBaseCharacter.h"
#include "ALSPlayerCharacter.generated.h"

/**
 * 
 */
UCLASS()
class ALSV4_CPP_API AALSPlayerCharacter : public AALSBaseCharacter
{
	GENERATED_BODY()

public:
	AALSPlayerCharacter(const FObjectInitializer& ObjectInitializer);

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	
	virtual void BeginPlay() override;

	virtual FVector GetMovementDirection() const override;

	virtual void OnRotationModeChanged(EALSRotationMode PreviousRotationMode) override;

	UFUNCTION(BlueprintCallable, Category = "ALS|Essential Information")
	void GetControlForwardRightVector(FVector& Forward, FVector& Right) const;

	/** Camera System */

	UFUNCTION(BlueprintGetter, Category = "ALS|Camera System")
    bool IsRightShoulder() const { return bRightShoulder; }

	UFUNCTION(BlueprintCallable, Category = "ALS|Camera System")
    void SetRightShoulder(const bool bNewRightShoulder) { bRightShoulder = bNewRightShoulder; }

	UFUNCTION(BlueprintCallable, Category = "ALS|Camera System")
    virtual ECollisionChannel GetThirdPersonTraceParams(FVector& TraceOrigin, float& TraceRadius);

	UFUNCTION(BlueprintCallable, Category = "ALS|Camera System")
    virtual FTransform GetThirdPersonPivotTarget();

	UFUNCTION(BlueprintCallable, Category = "ALS|Camera System")
    virtual FVector GetFirstPersonCameraTarget();

	UFUNCTION(BlueprintCallable, Category = "ALS|Camera System")
    void GetCameraParameters(float& TPFOVOut, float& FPFOVOut, bool& bRightShoulderOut) const;
	
protected:
	/** Input */

	UFUNCTION(BlueprintCallable, Category = "Input")
	void MovementInput_X(float Value);

	UFUNCTION(BlueprintCallable, Category = "Input")
    void MovementInput_Y(float Value);

	UFUNCTION(BlueprintCallable, Category = "Input")
	void MovementInput_Z(float Value);

	UFUNCTION(BlueprintCallable, Category = "Input")
    void Input_Jump();

	UFUNCTION(BlueprintCallable, Category = "Input")
    void Input_Jump_Release();

	UFUNCTION(BlueprintCallable, Category = "Input")
    void Input_Sprint();

	UFUNCTION(BlueprintCallable, Category = "Input")
    void Input_Sprint_Release();

	UFUNCTION(BlueprintCallable, Category = "Input")
    void Input_Aim();

	UFUNCTION(BlueprintCallable, Category = "Input")
    void Input_Aim_Release();

	UFUNCTION(BlueprintCallable, Category = "Input")
    void CameraPitchInput(float Value);

	UFUNCTION(BlueprintCallable, Category = "Input")
    void CameraYawInput(float Value);

	UFUNCTION(BlueprintCallable, Category = "Input")
    void CameraRollInput(float Value);

	UFUNCTION(BlueprintCallable, Category = "Input")
    void Camera_Action();

	UFUNCTION(BlueprintCallable, Category = "Input")
    void Camera_Action_Release();

	void OnSwitchCameraMode();

	UFUNCTION(BlueprintCallable, Category = "Input")
    void Input_Stance();

	UFUNCTION(BlueprintCallable, Category = "Input")
    void Input_Gait();

	UFUNCTION(BlueprintCallable, Category = "Input")
    void Input_Ragdoll();

	UFUNCTION(BlueprintCallable, Category = "Input")
    void Input_VelocityDirection();

	UFUNCTION(BlueprintCallable, Category = "Input")
    void Input_LookingDirection();

	/** View Mode state */
public:
	
	UFUNCTION(BlueprintCallable, Category = "ALS|Character States")
    void SetViewMode(EALSViewMode NewViewMode);

	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "ALS|Character States")
    void Server_SetViewMode(EALSViewMode NewViewMode);

	UFUNCTION(BlueprintGetter, Category = "ALS|Character States")
    EALSViewMode GetViewMode() const { return ViewMode; }
	
protected:
	
	virtual void OnViewModeChanged(EALSViewMode PreviousViewMode);

	UFUNCTION()
	void OnRep_ViewMode(EALSViewMode PrevViewMode);
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ALS|State Values", ReplicatedUsing = OnRep_ViewMode)
	EALSViewMode ViewMode = EALSViewMode::ThirdPerson;
	
	/** Input */

	// Cache the settings value for input axes.
	FName InputX, InputY, InputZ, CameraPitch, CameraRoll, CameraYaw;

	UPROPERTY(EditDefaultsOnly, Category = "ALS|Input", BlueprintReadOnly)
	float LookPitchRate = 1.25f;

	UPROPERTY(EditDefaultsOnly, Category = "ALS|Input", BlueprintReadOnly)
	float LookYawRate = 1.25f;

	UPROPERTY(EditDefaultsOnly, Category = "ALS|Input", BlueprintReadOnly)
	float LookRollRate = 1.25f;

	UPROPERTY(EditDefaultsOnly, Category = "ALS|Input", BlueprintReadOnly)
	float RollDoubleTapTimeout = 0.3f;
	
	UPROPERTY(EditDefaultsOnly, Category = "ALS|Input", BlueprintReadOnly)
	float ViewModeSwitchHoldTime = 0.2f;

	/** Camera System */

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ALS|Camera System")
	float ThirdPersonFOV = 90.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ALS|Camera System")
	float FirstPersonFOV = 90.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ALS|Camera System")
	bool bRightShoulder = false;
	
	/** Cached Variables */

	/** Last time the 'first' crouch/roll button is pressed */
	float LastStanceInputTime = 0.0f;
	
	/** Last time the camera action button is pressed */
	float CameraActionPressedTime = 0.0f;

	/* Timer to manage camera mode swap action */
	FTimerHandle OnCameraModeSwapTimer;
};