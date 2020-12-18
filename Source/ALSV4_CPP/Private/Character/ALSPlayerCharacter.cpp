// Fill out your copyright notice in the Description page of Project Settings.

#include "Character/ALSPlayerCharacter.h"
#include "Character/ALSCharacterMovementComponent.h"
#include "ALS_Settings.h"
#include "Character/Animation/ALSCharacterAnimInstance.h"
#include "Kismet/KismetMathLibrary.h"
#include "Library/ALSMathLibrary.h"
#include "Net/UnrealNetwork.h"

struct FALSAnimCharacterInformation;

AALSPlayerCharacter::AALSPlayerCharacter(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    const auto ALS_Settings = UALS_Settings::Get();
    InputX = ALS_Settings->X_Axis_Input;
    InputY = ALS_Settings->Y_Axis_Input;
    InputZ = ALS_Settings->Z_Axis_Input;
	CameraPitch = ALS_Settings->Pitch_Input;
	CameraYaw = ALS_Settings->Yaw_Input;
	CameraRoll = ALS_Settings->Roll_Input;
}

void AALSPlayerCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(AALSPlayerCharacter, ViewMode, COND_SkipOwner);
}

void AALSPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
    PlayerInputComponent->BindAxis(InputX, this, &AALSPlayerCharacter::MovementInput_X);
    PlayerInputComponent->BindAxis(InputY, this, &AALSPlayerCharacter::MovementInput_Y);
	PlayerInputComponent->BindAxis(InputZ, this, &AALSPlayerCharacter::MovementInput_Z);
	PlayerInputComponent->BindAxis(CameraPitch, this, &AALSPlayerCharacter::CameraPitchInput);
	PlayerInputComponent->BindAxis(CameraYaw, this, &AALSPlayerCharacter::CameraYawInput);
	PlayerInputComponent->BindAxis(CameraRoll, this, &AALSPlayerCharacter::CameraRollInput);
}

void AALSPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	FALSAnimCharacterInformation& AnimData = MainAnimInstance->GetCharacterInformationMutable();
	AnimData.ViewMode = ViewMode;
	SetViewMode(ViewMode);
}

FVector AALSPlayerCharacter::GetMovementDirection() const
{
    FVector Forward, Right = FVector::ZeroVector;
    GetControlForwardRightVector(Forward, Right);
    return (Forward + Right).GetSafeNormal();
}

void AALSPlayerCharacter::OnRotationModeChanged(const EALSRotationMode PreviousRotationMode)
{
	Super::OnRotationModeChanged(PreviousRotationMode);
	
	if (RotationMode == EALSRotationMode::VelocityDirection && ViewMode == EALSViewMode::FirstPerson)
	{
		// If the new rotation mode is Velocity Direction and the character is in First Person,
		// set the viewmode to Third Person.
		SetViewMode(EALSViewMode::ThirdPerson);
	}
}

void AALSPlayerCharacter::GetControlForwardRightVector(FVector& Forward, FVector& Right) const
{
    const FRotator ControlRot(0.0f, AimingRotation.Yaw, 0.0f);
    Forward = GetInputAxisValue(InputX) * UKismetMathLibrary::GetForwardVector(ControlRot);
    Right = GetInputAxisValue(InputZ) * UKismetMathLibrary::GetRightVector(ControlRot);
}

ECollisionChannel AALSPlayerCharacter::GetThirdPersonTraceParams(FVector& TraceOrigin, float& TraceRadius)
{
    TraceOrigin = GetActorLocation();
    TraceRadius = 10.0f;
    return ECC_Visibility;
}

FTransform AALSPlayerCharacter::GetThirdPersonPivotTarget()
{
    return GetActorTransform();
}

FVector AALSPlayerCharacter::GetFirstPersonCameraTarget()
{
    return GetMesh()->GetSocketLocation(FName(TEXT("FP_Camera")));
}

void AALSPlayerCharacter::GetCameraParameters(float& TPFOVOut, float& FPFOVOut, bool& bRightShoulderOut) const
{
    TPFOVOut = ThirdPersonFOV;
    FPFOVOut = FirstPersonFOV;
    bRightShoulderOut = bRightShoulder;
}

void AALSPlayerCharacter::MovementInput_X(const float Value)
{
    if (GetInputAxisValue(InputX) == 0 && Value == 0) return;
	
    // Default camera relative movement behavior
    const float Scale = UALSMathLibrary::FixDiagonalGamepadValues(Value, GetInputAxisValue(InputY)).Key;

	float Pitch = AimingRotation.Pitch;
	if (GetCharacterMovement()->MovementMode == MOVE_Flying)
	{
		if (Value >= 0)
		{
			Pitch = FMath::ClampAngle(AimingRotation.Pitch, -MaxFlightForwardAngle, MaxFlightForwardAngle * GetAtmospherePressure());
		}
		else if (Value < 0)
		{
			Pitch = FMath::ClampAngle(AimingRotation.Pitch, -MaxFlightForwardAngle * GetAtmospherePressure(), MaxFlightForwardAngle);
		}
	}
    const FRotator DirRotator(Pitch, AimingRotation.Yaw, 0.0f);
    AddMovementInput(UKismetMathLibrary::GetForwardVector(DirRotator), Scale);
}

void AALSPlayerCharacter::MovementInput_Y(const float Value)
{
    if (GetInputAxisValue(InputY) == 0 && Value == 0) return;

    // Default camera relative movement behavior
    const float Scale = UALSMathLibrary::FixDiagonalGamepadValues(GetInputAxisValue(InputX), Value).Value;
    const FRotator DirRotator(0.0f, AimingRotation.Yaw, 0.0f);
    AddMovementInput(UKismetMathLibrary::GetRightVector(DirRotator), Scale);
}

void AALSPlayerCharacter::MovementInput_Z(const float Value)
{
	if (GetInputAxisValue(InputZ) == 0 && Value == 0) return;
	
	// Default camera relative movement behavior
	const FRotator DirRotator(0.0f, AimingRotation.Yaw, 0.0f);
	AddMovementInput(UKismetMathLibrary::GetUpVector(DirRotator), Value);
}

void AALSPlayerCharacter::CameraPitchInput(const float Value)
{
	AddControllerPitchInput(LookPitchRate * Value);
}

void AALSPlayerCharacter::CameraYawInput(const float Value)
{
	AddControllerYawInput(LookYawRate * Value);
}

void AALSPlayerCharacter::CameraRollInput(const float Value)
{
	AddControllerRollInput(LookRollRate * Value);
}

void AALSPlayerCharacter::Input_Jump()
{
	// Jump Action: Press "Jump Action" to end the ragdoll if ragdolling, check for a mantle if grounded or in air,
	// stand up if crouching, or jump if standing.

	if (MovementAction != EALSMovementAction::None) return; // Guard against jumping while doing something else.
	
	if (MovementState == EALSMovementState::Grounded)
	{
		if (bHasMovementInput)
		{
			if (MantleCheckGrounded()) { return; } // Check to see if there is a mantle to perform instead.
		}
		if (Stance == EALSStance::Standing)
		{
			Jump();
		}
		else if (Stance == EALSStance::Crouching)
		{
			UnCrouch();
		}
	}
	else if (MovementState == EALSMovementState::Freefall)
	{
		if (MantleCheckFalling()) return;

		SetFlightMode(EALSFlightMode::Neutral);
	}
	else if (MovementState == EALSMovementState::Flight)
	{
		if (FlightMode == EALSFlightMode::Lowering)
		{
			SetFlightMode(EALSFlightMode::Neutral);
		}
	}
	else if (MovementState == EALSMovementState::Ragdoll)
	{
		ReplicatedRagdollEnd();
	}
}

void AALSPlayerCharacter::Input_Jump_Release()
{
	StopJumping();
}

void AALSPlayerCharacter::Input_Sprint()
{
	SetDesiredGait(EALSGait::GaitFast);
}

void AALSPlayerCharacter::Input_Sprint_Release()
{
	SetDesiredGait(EALSGait::GaitNormal);
}

void AALSPlayerCharacter::Input_Aim()
{
	// AimAction: Hold "AimAction" to enter the aiming mode, release to revert back the desired rotation mode.
	SetRotationMode(EALSRotationMode::Aiming);
}

void AALSPlayerCharacter::Input_Aim_Release()
{
	if (ViewMode == EALSViewMode::ThirdPerson)
	{
		SetRotationMode(DesiredRotationMode);
	}
	else if (ViewMode == EALSViewMode::FirstPerson)
	{
		SetRotationMode(EALSRotationMode::LookingDirection);
	}
}

void AALSPlayerCharacter::Camera_Action()
{
	UWorld* World = GetWorld();
	check(World);
	CameraActionPressedTime = World->GetTimeSeconds();
	GetWorldTimerManager().SetTimer(OnCameraModeSwapTimer, this,
	                                &AALSPlayerCharacter::OnSwitchCameraMode, ViewModeSwitchHoldTime, false);
}

void AALSPlayerCharacter::Camera_Action_Release()
{
	if (ViewMode == EALSViewMode::FirstPerson) { return; } // Don't swap shoulders on first person mode

	UWorld* World = GetWorld();
	check(World);
	if (World->GetTimeSeconds() - CameraActionPressedTime < ViewModeSwitchHoldTime)
	{
		// Switch shoulders
		SetRightShoulder(!bRightShoulder);
		GetWorldTimerManager().ClearTimer(OnCameraModeSwapTimer); // Prevent mode change
	}
}

void AALSPlayerCharacter::OnSwitchCameraMode()
{
	// Switch camera mode
	if (ViewMode == EALSViewMode::FirstPerson)
	{
		SetViewMode(EALSViewMode::ThirdPerson);
	}
	else if (ViewMode == EALSViewMode::ThirdPerson)
	{
		SetViewMode(EALSViewMode::FirstPerson);
	}
}

void AALSPlayerCharacter::Input_Stance()
{
	// Stance Action: Press "Stance Action" to toggle Standing / Crouching, double tap to Roll.

	if (MovementAction != EALSMovementAction::None) return;

	UWorld* World = GetWorld();
	check(World);

	const float PrevStanceInputTime = LastStanceInputTime;
	LastStanceInputTime = World->GetTimeSeconds();

	if (LastStanceInputTime - PrevStanceInputTime <= RollDoubleTapTimeout)
	{
		// Roll
		Replicated_PlayMontage(GetRollAnimation(), 1.15f);

		if (Stance == EALSStance::Standing)
		{
			SetDesiredStance(EALSStance::Crouching);
		}
		else if (Stance == EALSStance::Crouching)
		{
			SetDesiredStance(EALSStance::Standing);
		}
		return;
	}

	if (MovementState == EALSMovementState::Grounded)
	{
		if (Stance == EALSStance::Standing)
		{
			SetDesiredStance(EALSStance::Crouching);
			Crouch();
		}
		else if (Stance == EALSStance::Crouching)
		{
			SetDesiredStance(EALSStance::Standing);
			UnCrouch();
		}
	}
}

void AALSPlayerCharacter::Input_Gait()
{
	if (DesiredGait == EALSGait::GaitSlow)
	{
		SetDesiredGait(EALSGait::GaitNormal);
	}
	else if (DesiredGait == EALSGait::GaitNormal)
	{
		SetDesiredGait(EALSGait::GaitSlow);
	}
}

void AALSPlayerCharacter::Input_Ragdoll()
{
	// Ragdoll Action: Press "Ragdoll Action" to toggle the ragdoll state on or off.

	if (GetMovementState() == EALSMovementState::Ragdoll)
	{
		ReplicatedRagdollEnd();
	}
	else
	{
		ReplicatedRagdollStart();
	}
}

void AALSPlayerCharacter::Input_VelocityDirection()
{
	// Select Rotation Mode: Switch the desired (default) rotation mode to Velocity or Looking Direction.
	// This will be the mode the character reverts back to when un-aiming
	SetDesiredRotationMode(EALSRotationMode::VelocityDirection);
	SetRotationMode(EALSRotationMode::VelocityDirection);
}

void AALSPlayerCharacter::Input_LookingDirection()
{
	SetDesiredRotationMode(EALSRotationMode::LookingDirection);
	SetRotationMode(EALSRotationMode::LookingDirection);
}

void AALSPlayerCharacter::SetViewMode(const EALSViewMode NewViewMode)
{
	if (ViewMode != NewViewMode)
	{
		const EALSViewMode Prev = ViewMode;
		ViewMode = NewViewMode;
		OnViewModeChanged(Prev);

		if (GetLocalRole() == ROLE_AutonomousProxy)
		{
			Server_SetViewMode(NewViewMode);
		}
	}
}

void AALSPlayerCharacter::Server_SetViewMode_Implementation(const EALSViewMode NewViewMode)
{
	SetViewMode(NewViewMode);
}

void AALSPlayerCharacter::OnViewModeChanged(const EALSViewMode PreviousViewMode)
{
	MainAnimInstance->GetCharacterInformationMutable().ViewMode = ViewMode;
	switch (ViewMode)
	{
	case EALSViewMode::ThirdPerson:
		if (RotationMode == EALSRotationMode::VelocityDirection || RotationMode == EALSRotationMode::LookingDirection)
		{
			// If Third Person, set the rotation mode back to the desired mode.
			SetRotationMode(DesiredRotationMode);
		}
		RestrictAiming = RotationMode == EALSRotationMode::Aiming;break;
	case EALSViewMode::FirstPerson:
		{
			RestrictAiming = true;
			if (RotationMode == EALSRotationMode::VelocityDirection)
			{
				// If First Person, set the rotation mode to looking direction if currently in the velocity direction mode.
				SetRotationMode(EALSRotationMode::LookingDirection);
			}
		}
		break;
	default: ;
	}
	K2_OnViewModeChanged(PreviousViewMode);
	ViewModeChangedDelegate.Broadcast(this, PreviousViewMode);
}

void AALSPlayerCharacter::OnRep_ViewMode(const EALSViewMode PrevViewMode)
{
	OnViewModeChanged(PrevViewMode);
}
