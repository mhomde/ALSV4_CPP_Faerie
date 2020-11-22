// Project:         Advanced Locomotion System V4 on C++
// Copyright:       Copyright (C) 2020 Doğa Can Yanıkoğlu
// License:         MIT License (http://www.opensource.org/licenses/mit-license.php)
// Source Code:     https://github.com/dyanikoglu/ALSV4_CPP
// Original Author: Doğa Can Yanıkoğlu
// Contributors:    Haziq Fadhil


#include "Character/ALSBaseCharacter.h"
#include "ALS_Settings.h"
#include "DrawDebugHelpers.h"
#include "Character/ALSPlayerController.h"
#include "Character/Animation/ALSCharacterAnimInstance.h"
#include "Library/ALSMathLibrary.h"
#include "Components/CapsuleComponent.h"
#include "Components/TimelineComponent.h"
#include "Curves/CurveVector.h"
#include "Curves/CurveFloat.h"
#include "Character/ALSCharacterMovementComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Net/UnrealNetwork.h"

AALSBaseCharacter::AALSBaseCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UALSCharacterMovementComponent>(CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = true;
	MantleTimeline = CreateDefaultSubobject<UTimelineComponent>(FName(TEXT("MantleTimeline")));
	bUseControllerRotationYaw = 0;
	SetReplicates(true);
	SetReplicatingMovement(true);
	const auto UALS_Settings = UALS_Settings::Get();
	SeaAltitude = UALS_Settings->SeaAltitude;
	TroposphereHeight = UALS_Settings->TroposphereHeight;
}

void AALSBaseCharacter::Restart()
{
	Super::Restart();

	AALSPlayerController* NewController = Cast<AALSPlayerController>(GetController());
	if (NewController)
	{
		NewController->OnRestartPawn(this);
	}
}

void AALSBaseCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	MyCharacterMovementComponent = Cast<UALSCharacterMovementComponent>(Super::GetMovementComponent());
}

void AALSBaseCharacter::NotifyHit(UPrimitiveComponent* MyComp,
								  AActor* Other,
								  UPrimitiveComponent* OtherComp,
								  const bool bSelfMoved,
								  const FVector HitLocation,
								  const FVector HitNormal,
								  const FVector NormalImpulse,
								  const FHitResult& Hit)
{
	Super::NotifyHit(MyComp, Other, OtherComp, bSelfMoved, HitLocation, HitNormal, NormalImpulse, Hit);
	
	if (FlightMode != EALSFlightMode::None)
	{
		switch (FlightCancelCondition)
		{
		case EALSFlightCancelCondition::Disabled: break;
		case EALSFlightCancelCondition::AnyHit:
			SetFlightMode(EALSFlightMode::None);
			break;
		case EALSFlightCancelCondition::VelocityThreshold:
			if (FlightInterruptThresholdCheck())
			{
				SetFlightMode(EALSFlightMode::None);
			}
			break;
		case EALSFlightCancelCondition::Custom:
			if (FlightInterruptCustomCheck(MyComp, Other, OtherComp, bSelfMoved, HitLocation, HitNormal, NormalImpulse, Hit))
			{
				SetFlightMode(EALSFlightMode::None);
			}
			break;
		case EALSFlightCancelCondition::CustomOrThreshold:
			if (FlightInterruptThresholdCheck()
				|| FlightInterruptCustomCheck(MyComp, Other, OtherComp, bSelfMoved, HitLocation, HitNormal, NormalImpulse, Hit))
			{
				SetFlightMode(EALSFlightMode::None);
			}
			break;
		case EALSFlightCancelCondition::CustomAndThreshold:
			if (FlightInterruptThresholdCheck()
				&& FlightInterruptCustomCheck(MyComp, Other, OtherComp, bSelfMoved, HitLocation, HitNormal, NormalImpulse, Hit))
			{
				SetFlightMode(EALSFlightMode::None);
			}
			break;
		default: ;
		}
	}
}

void AALSBaseCharacter::AddMovementInput(FVector WorldDirection, const float ScaleValue, const bool bForce)
{
	if (GetCharacterMovement()->MovementMode == MOVE_Flying)
	{
		if (WorldDirection.Z > 0.0f)
		{
			WorldDirection.Z *= GetAtmospherePressure();
		}
	}
	Super::AddMovementInput(WorldDirection, ScaleValue, bForce);
}

void AALSBaseCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AALSBaseCharacter, TargetRagdollLocation);
	DOREPLIFETIME_CONDITION(AALSBaseCharacter, ReplicatedCurrentAcceleration, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(AALSBaseCharacter, ReplicatedControlRotation, COND_SkipOwner);

	DOREPLIFETIME(AALSBaseCharacter, DesiredGait);
	DOREPLIFETIME_CONDITION(AALSBaseCharacter, DesiredStance, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(AALSBaseCharacter, DesiredRotationMode, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(AALSBaseCharacter, RotationMode, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(AALSBaseCharacter, OverlayState, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(AALSBaseCharacter, FlightMode, COND_SkipOwner);
}

void AALSBaseCharacter::BeginPlay()
{
	Super::BeginPlay();

	// If we're in networked game, use this to disable curved movement
	bIsNetworked = !IsNetMode(NM_Standalone);

	FOnTimelineFloat TimelineUpdated;
	FOnTimelineEvent TimelineFinished;
	TimelineUpdated.BindUFunction(this, FName(TEXT("MantleUpdate")));
	TimelineFinished.BindUFunction(this, FName(TEXT("MantleEnd")));
	MantleTimeline->SetTimelineFinishedFunc(TimelineFinished);
	MantleTimeline->SetLooping(false);
	MantleTimeline->SetTimelineLengthMode(TL_TimelineLength);
	MantleTimeline->AddInterpFloat(MantleTimelineCurve, TimelineUpdated);

	// Make sure the mesh and animbp update after the CharacterBP to ensure it gets the most recent values.
	GetMesh()->AddTickPrerequisiteActor(this);

	// Set the Movement Model
	SetMovementModel();

	// Once, force set variables in anim bp. This ensures anim instance & character starts synchronized
	FALSAnimCharacterInformation& AnimData = MainAnimInstance->GetCharacterInformationMutable();
	MainAnimInstance->Gait = DesiredGait;
	MainAnimInstance->Stance = DesiredStance;
	MainAnimInstance->RotationMode = DesiredRotationMode;
	MainAnimInstance->OverlayState = OverlayState;
	AnimData.PrevMovementState = PrevMovementState;
	MainAnimInstance->MovementState = MovementState;

	// Update states to use the initial desired values.
	SetGait(DesiredGait);
	SetStance(DesiredStance);
	SetRotationMode(DesiredRotationMode);
	SetOverlayState(OverlayState);

	if (Stance == EALSStance::Standing)
	{
		UnCrouch();
	}
	else if (Stance == EALSStance::Crouching)
	{
		Crouch();
	}

	// Set default rotation values.
	TargetRotation = GetActorRotation();
	LastVelocityRotation = TargetRotation;
	LastMovementInputRotation = TargetRotation;

	if (GetLocalRole() == ROLE_SimulatedProxy)
	{
		MainAnimInstance->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
	}
}

void AALSBaseCharacter::PreInitializeComponents()
{
	Super::PreInitializeComponents();

	MainAnimInstance = Cast<UALSCharacterAnimInstance>(GetMesh()->GetAnimInstance());
	if (!MainAnimInstance)
	{
		// Animation instance should be assigned if we're not in editor preview
		checkf(GetWorld()->WorldType == EWorldType::EditorPreview,
		       TEXT("%s doesn't have a valid animation instance assigned. That's not allowed"),
		       *GetName());
	}
}

void AALSBaseCharacter::Tick(const float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Set required values
	SetEssentialValues(DeltaTime);

	switch (MovementState) 
	{
	case EALSMovementState::None: break;
	case EALSMovementState::Grounded: 
		UpdateCharacterMovement(DeltaTime);
		UpdateGroundedRotation(DeltaTime);
		
		// Perform a mantle check for short objects while movement input is pressed.
		if (bHasMovementInput && bUseAutoVault && IsLocallyControlled())
		{
			MantleCheckVault();
		}
		break;
	case EALSMovementState::Freefall:
		UpdateFallingRotation(DeltaTime);

		// Perform a mantle check if falling while movement input is pressed or the constant check flag is true.
		if (bHasMovementInput || bAlwaysCatchIfFalling)
		{
			MantleCheckFalling();
		}
		break;
	case EALSMovementState::Flight:
		UpdateRelativeAltitude();
		UpdateCharacterMovement(DeltaTime);
		UpdateFlightRotation(DeltaTime);
		if (HasAuthority() || GetLocalRole() == ROLE_AutonomousProxy)
		{
			UpdateFlightMovement(DeltaTime);
		}
		break;
	case EALSMovementState::Swimming:
		UpdateCharacterMovement(DeltaTime);
		UpdateSwimmingRotation(DeltaTime);
		break;
	case EALSMovementState::Mantling: break;
	case EALSMovementState::Ragdoll:
		RagdollUpdate(DeltaTime);
		break;
	default: break;
	}

	// Cache values
	PreviousVelocity = GetVelocity();
	PreviousAimYaw = AimingRotation.Yaw;

	DrawDebugSpheres();
}

void AALSBaseCharacter::SetAimYawRate(const float NewAimYawRate)
{
	AimYawRate = NewAimYawRate;
	MainAnimInstance->GetCharacterInformationMutable().AimYawRate = AimYawRate;
}

void AALSBaseCharacter::RagdollStart()
{
	/**
	 * When Networked, disables replicate movement reset TargetRagdollLocation and ServerRagdollPull variable and if
	 * the host is a dedicated server, change character mesh optimisation option to avoid z-location issue.
	*/
	MyCharacterMovementComponent->bIgnoreClientMovementErrorChecksAndCorrection = 1;

	if (UKismetSystemLibrary::IsDedicatedServer(GetWorld()))
	{
		DefVisBasedTickOp = GetMesh()->VisibilityBasedAnimTickOption;
		GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	}
	TargetRagdollLocation = GetMesh()->GetSocketLocation(FName(TEXT("Pelvis")));
	ServerRagdollPull = 0;

	// Step 1: Clear the Character Movement Mode and set the Movement State to Ragdoll
	GetCharacterMovement()->SetMovementMode(MOVE_None);
	SetMovementState(EALSMovementState::Ragdoll);

	// Step 2: Disable capsule collision and enable mesh physics simulation starting from the pelvis.
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetMesh()->SetCollisionObjectType(ECC_PhysicsBody);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	GetMesh()->SetAllBodiesBelowSimulatePhysics(FName(TEXT("Pelvis")), true, true);

	// Step 3: Stop any active montages.
	MainAnimInstance->Montage_Stop(0.2f);

	SetReplicateMovement(false);
}

void AALSBaseCharacter::RagdollEnd()
{
	/** Re-enable Replicate Movement and if the host is a dedicated server set mesh visibility based anim
	tick option back to default*/

	if (UKismetSystemLibrary::IsDedicatedServer(GetWorld()))
	{
		GetMesh()->VisibilityBasedAnimTickOption = DefVisBasedTickOp;
	}

	MyCharacterMovementComponent->bIgnoreClientMovementErrorChecksAndCorrection = 0;
	SetReplicateMovement(true);

	if (!MainAnimInstance) { return; }

	// Step 1: Save a snapshot of the current Ragdoll Pose for use in AnimGraph to blend out of the ragdoll
	MainAnimInstance->SavePoseSnapshot(FName(TEXT("RagdollPose")));

	// Step 2: If the ragdoll is on the ground, set the movement mode to walking and play a Get Up animation.
	// If not, set the movement mode to falling and update the character movement velocity to match the last ragdoll velocity.
	if (bRagdollOnGround)
	{
		GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		MainAnimInstance->Montage_Play(GetGetUpAnimation(bRagdollFaceUp),
		                               1.0f, EMontagePlayReturnType::MontageLength, 0.0f, true);
	}
	else
	{
		GetCharacterMovement()->SetMovementMode(MOVE_Falling);
		GetCharacterMovement()->Velocity = LastRagdollVelocity;
	}

	// Step 3: Re-Enable capsule collision, and disable physics simulation on the mesh.
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	GetMesh()->SetCollisionObjectType(ECC_Pawn);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	GetMesh()->SetAllBodiesSimulatePhysics(false);
}

void AALSBaseCharacter::SetMovementState(const EALSMovementState NewState)
{
	if (MovementState != NewState)
	{
		PrevMovementState = MovementState;
		MovementState = NewState;
		FALSAnimCharacterInformation& AnimData = MainAnimInstance->GetCharacterInformationMutable();
		AnimData.PrevMovementState = PrevMovementState;
		MainAnimInstance->MovementState = MovementState;
		OnMovementStateChanged(PrevMovementState);
	}
}

void AALSBaseCharacter::SetMovementAction(const EALSMovementAction NewAction)
{
	if (MovementAction != NewAction)
	{
		const EALSMovementAction Prev = MovementAction;
		MovementAction = NewAction;
		MainAnimInstance->MovementAction = MovementAction;
		OnMovementActionChanged(Prev);
	}
}

void AALSBaseCharacter::SetStance(const EALSStance NewStance)
{
	if (Stance != NewStance)
	{
		const EALSStance Prev = Stance;
		Stance = NewStance;
		MainAnimInstance->Stance = Stance;
		OnStanceChanged(Prev);
	}
}

void AALSBaseCharacter::SetGait(const EALSGait NewGait)
{
	if (Gait != NewGait)
	{
		Gait = NewGait;
		MainAnimInstance->Gait = Gait;
	}
}

void AALSBaseCharacter::SetDesiredStance(const EALSStance NewStance)
{
	DesiredStance = NewStance;
	if (GetLocalRole() == ROLE_AutonomousProxy)
	{
		Server_SetDesiredStance(NewStance);
	}
}

void AALSBaseCharacter::SetDesiredGait(const EALSGait NewGait)
{
	DesiredGait = NewGait;
	if (GetLocalRole() == ROLE_AutonomousProxy)
	{
		Server_SetDesiredGait(NewGait);
	}
}

void AALSBaseCharacter::SetDesiredRotationMode(const EALSRotationMode NewRotMode)
{
	DesiredRotationMode = NewRotMode;

	if (GetLocalRole() == ROLE_AutonomousProxy)
	{
		Server_SetDesiredRotationMode(NewRotMode);
	}
}

void AALSBaseCharacter::SetRotationMode(const EALSRotationMode NewRotationMode)
{
	if (RotationMode != NewRotationMode)
	{
		const EALSRotationMode Prev = RotationMode;
		RotationMode = NewRotationMode;
		OnRotationModeChanged(Prev);

		if (GetLocalRole() == ROLE_AutonomousProxy)
		{
			Server_SetRotationMode(NewRotationMode);
		}
	}
}

void AALSBaseCharacter::SetFlightMode(const EALSFlightMode NewFlightMode)
{
	if (NewFlightMode == FlightMode) return; // Guard to prevent useless calls.

	// If we are trying for a mode other than turning flight off, verify the character is able to fly.
	if (NewFlightMode != EALSFlightMode::None)
	{
		if (!CanFly()) return;
	}

	// With guards passed we can set the flying mode.
	
	const EALSFlightMode Prev = FlightMode;
	FlightMode = NewFlightMode;
	OnFlightModeChanged(Prev);

	if (GetLocalRole() == ROLE_AutonomousProxy)
	{
		Server_SetFlightMode(NewFlightMode);
	}
}

void AALSBaseCharacter::SetOverlayState(const EALSOverlayState NewState)
{
	if (OverlayState != NewState)
	{
		const EALSOverlayState Prev = OverlayState;
		OverlayState = NewState;
		OnOverlayStateChanged(Prev);

		if (GetLocalRole() == ROLE_AutonomousProxy)
		{
			Server_SetOverlayState(NewState);
		}
	}
}

void AALSBaseCharacter::EventOnLanded()
{
	const float VelZ = FMath::Abs(GetCharacterMovement()->Velocity.Z);

	if (bRagdollOnLand && VelZ > RagdollOnLandVelocity)
	{
		ReplicatedRagdollStart();
	}
	else if (bBreakfallOnLand && ((bHasMovementInput && VelZ >= BreakfallOnLandVelocity) || bBreakFallNextLanding))
	{
		OnBreakfall();
		bBreakFallNextLanding = false;
	}
	else
	{
		GetCharacterMovement()->BrakingFrictionFactor = bHasMovementInput ? 0.5f : 3.0f;

		// After 0.5 secs, reset braking friction factor to zero
		GetWorldTimerManager().SetTimer(OnLandedFrictionResetTimer, this,
		                                &AALSBaseCharacter::OnLandFrictionReset, 0.5f, false);
	}
}

void AALSBaseCharacter::EventOnJumped()
{
	// Set the new In Air Rotation to the velocity rotation if speed is greater than 100.
	InAirRotation = Speed > 100.0f ? LastVelocityRotation : GetActorRotation();
	MainAnimInstance->OnJumped();
}

void AALSBaseCharacter::SetActorLocationAndTargetRotation(const FVector NewLocation, const FRotator NewRotation)
{
	SetActorLocationAndRotation(NewLocation, NewRotation);
	TargetRotation = NewRotation;
}

bool AALSBaseCharacter::MantleCheckGrounded()
{
	return MantleCheck(GroundedTraceSettings);
}

bool AALSBaseCharacter::MantleCheckFalling()
{
	return MantleCheck(FallingTraceSettings);
}

bool AALSBaseCharacter::MantleCheckVault()
{
	// Grab and cache the automatic trace settings.
	FALSMantleTraceSettings MantleTrace = AutomaticTraceSettings;

	// Adjust for seemless vaulting.
	MantleTrace.MinLedgeHeight = GetCharacterMovement()->MaxStepHeight; // Ensures that the autovault will always trigger at the height that stepping up cuts out.

	// Attempt mantle.
	return MantleCheck(MantleTrace);
}

void AALSBaseCharacter::SetMovementModel()
{
	const FString ContextString = GetFullName();
	FALSMovementStateSettings* OutRow =
		MovementModel.DataTable->FindRow<FALSMovementStateSettings>(MovementModel.RowName, ContextString);
	check(OutRow);
	MovementData = *OutRow;
}

void AALSBaseCharacter::SetHasMovementInput(const bool bNewHasMovementInput)
{
	bHasMovementInput = bNewHasMovementInput;
	MainAnimInstance->GetCharacterInformationMutable().bHasMovementInput = bHasMovementInput;
}

FALSMovementSettings AALSBaseCharacter::GetTargetMovementSettings() const
{
	if (RotationMode == EALSRotationMode::VelocityDirection)
	{
		if (MovementState == EALSMovementState::Grounded)
		{
			if (Stance == EALSStance::Standing)
			{
				return MovementData.VelocityDirection.Standing;
			}
			if (Stance == EALSStance::Crouching)
			{
				return MovementData.VelocityDirection.Crouching;
			}
		}
		if (MovementState == EALSMovementState::Flight)
		{
			return MovementData.VelocityDirection.Flying;
		}
		if (MovementState == EALSMovementState::Swimming)
		{
			return MovementData.VelocityDirection.Swimming;
		}
	}
	else if (RotationMode == EALSRotationMode::LookingDirection)
	{
		if (MovementState == EALSMovementState::Grounded)
		{
			if (Stance == EALSStance::Standing)
			{
				return MovementData.LookingDirection.Standing;
			}
			if (Stance == EALSStance::Crouching)
			{
				return MovementData.LookingDirection.Crouching;
			}
		}
		if (MovementState == EALSMovementState::Flight)
		{
			return MovementData.LookingDirection.Flying;
		}
		if (MovementState == EALSMovementState::Swimming)
		{
			return MovementData.LookingDirection.Swimming;
		}
	}
	else if (RotationMode == EALSRotationMode::Aiming)
	{
		if (MovementState == EALSMovementState::Grounded)
		{
			if (Stance == EALSStance::Standing)
			{
				return MovementData.Aiming.Standing;
			}
			if (Stance == EALSStance::Crouching)
			{
				return MovementData.Aiming.Crouching;
			}
		}
		if (MovementState == EALSMovementState::Flight)
		{
			return MovementData.Aiming.Flying;
		}
		if (MovementState == EALSMovementState::Swimming)
		{
			return MovementData.Aiming.Swimming;
		}
	}

	// Default to velocity dir standing
	return MovementData.VelocityDirection.Standing;
}

bool AALSBaseCharacter::CanSprint() const
{
	// Determine if the character is currently able to sprint based on the Rotation mode and current acceleration
	// (input) rotation. If the character is in the Looking Rotation mode, only allow sprinting if there is full
	// movement input and it is faced forward relative to the camera + or - 50 degrees.

	if (!bHasMovementInput || RotationMode == EALSRotationMode::Aiming) { return false; } // Initial fail condition

	const bool bValidInputAmount = MovementInputAmount > 0.9f;

	if (RotationMode == EALSRotationMode::VelocityDirection)
	{
		return bValidInputAmount;
	}

	if (RotationMode == EALSRotationMode::LookingDirection)
	{
		const FRotator AccRot = ReplicatedCurrentAcceleration.ToOrientationRotator();
		FRotator Delta = AccRot - AimingRotation;
		Delta.Normalize();

		return bValidInputAmount && FMath::Abs(Delta.Yaw) < 50.0f;
	}

	return false;
}

void AALSBaseCharacter::SetIsMoving(const bool bNewIsMoving)
{
	bIsMoving = bNewIsMoving;
	MainAnimInstance->GetCharacterInformationMutable().bIsMoving = bIsMoving;
}

FVector AALSBaseCharacter::GetMovementInput() const
{
	return ReplicatedCurrentAcceleration;
}

void AALSBaseCharacter::SetMovementInputAmount(const float NewMovementInputAmount)
{
	MovementInputAmount = NewMovementInputAmount;
	MainAnimInstance->GetCharacterInformationMutable().MovementInputAmount = MovementInputAmount;
}

void AALSBaseCharacter::SetSpeed(const float NewSpeed)
{
	Speed = NewSpeed;
	MainAnimInstance->GetCharacterInformationMutable().Speed = Speed;
}

float AALSBaseCharacter::GetAnimCurveValue(const FName CurveName) const
{
	if (MainAnimInstance)
	{
		return MainAnimInstance->GetCurveValue(CurveName);
	}

	return 0.0f;
}

float AALSBaseCharacter::GetAbsoluteAltitude() const
{
	return GetActorLocation().Z - SeaAltitude;
}

float AALSBaseCharacter::GetAtmospherePressure() const
{
	// If AtmosphericPressureFalloff, then atmosphere falloff is considered disabled and will always return 1.
	if (!AtmosphericPressureFalloff) return 1;
	return AtmosphericPressureFalloff->GetFloatValue(GetAbsoluteAltitude() / TroposphereHeight);
}

FVector AALSBaseCharacter::GetInputAcceleration() const
{
	return GetActorRotation().UnrotateVector(ReplicatedCurrentAcceleration.GetSafeNormal());
}

void AALSBaseCharacter::SetAcceleration(const FVector& NewAcceleration)
{
	Acceleration = (NewAcceleration != FVector::ZeroVector || IsLocallyControlled()) ? NewAcceleration : Acceleration / 2;
	MainAnimInstance->GetCharacterInformationMutable().Acceleration = Acceleration;
}

void AALSBaseCharacter::RagdollUpdate(const float DeltaTime)
{
	// Set the Last Ragdoll Velocity.
	const FVector NewRagdollVel = GetMesh()->GetPhysicsLinearVelocity(FName(TEXT("root")));
	LastRagdollVelocity = NewRagdollVel != FVector::ZeroVector || IsLocallyControlled() ? NewRagdollVel : LastRagdollVelocity / 2;

	// Use the Ragdoll Velocity to scale the ragdoll's joint strength for physical animation.
	const float SpringValue = FMath::GetMappedRangeValueClamped({0.0f, 1000.0f}, {0.0f, 25000.0f}, LastRagdollVelocity.Size());
	GetMesh()->SetAllMotorsAngularDriveParams(SpringValue, 0.0f, 0.0f, false);

	// Disable Gravity if falling faster than -4000 to prevent continual acceleration.
	// This also prevents the ragdoll from going through the floor.
	const bool bEnableGrav = LastRagdollVelocity.Z > -4000.0f;
	GetMesh()->SetEnableGravity(bEnableGrav);

	// Update the Actor location to follow the ragdoll.
	SetActorLocationDuringRagdoll(DeltaTime);
}

void AALSBaseCharacter::SetActorLocationDuringRagdoll(const float DeltaTime)
{
	if (IsLocallyControlled())
	{
		// Set the pelvis as the target location.
		TargetRagdollLocation = GetMesh()->GetSocketLocation(FName(TEXT("Pelvis")));
		if (!HasAuthority())
		{
			Server_SetMeshLocationDuringRagdoll(TargetRagdollLocation);
		}
	}

	// Determine wether the ragdoll is facing up or down and set the target rotation accordingly.
	const FRotator PelvisRot = GetMesh()->GetSocketRotation(FName(TEXT("Pelvis")));

	bRagdollFaceUp = PelvisRot.Roll < 0.0f;

	const FRotator TargetRagdollRotation(0.0f, bRagdollFaceUp ? PelvisRot.Yaw - 180.0f : PelvisRot.Yaw, 0.0f);

	// Trace downward from the target location to offset the target location,
	// preventing the lower half of the capsule from going through the floor when the ragdoll is laying on the ground.
	const FVector TraceVect(TargetRagdollLocation.X, TargetRagdollLocation.Y,
	                        TargetRagdollLocation.Z - GetCapsuleComponent()->GetScaledCapsuleHalfHeight());

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);

	FHitResult HitResult;
	GetWorld()->LineTraceSingleByChannel(HitResult, TargetRagdollLocation, TraceVect,
	                                     ECC_Visibility, Params);

	bRagdollOnGround = HitResult.IsValidBlockingHit();
	FVector NewRagdollLoc = TargetRagdollLocation;

	if (bRagdollOnGround)
	{
		const float ImpactDistZ = FMath::Abs(HitResult.ImpactPoint.Z - HitResult.TraceStart.Z);
		NewRagdollLoc.Z += GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - ImpactDistZ + 2.0f;
	}
	if (!IsLocallyControlled())
	{
		ServerRagdollPull = FMath::FInterpTo(ServerRagdollPull, 750, DeltaTime, 0.6);
		float RagdollSpeed = FVector(LastRagdollVelocity.X, LastRagdollVelocity.Y, 0).Size();
		FName RagdollSocketPullName = RagdollSpeed > 300 ? FName(TEXT("spine_03")) : FName(TEXT("pelvis"));
		GetMesh()->AddForce((TargetRagdollLocation - GetMesh()->GetSocketLocation(RagdollSocketPullName)) * ServerRagdollPull,
		                    RagdollSocketPullName, true);
	}
	SetActorLocationAndTargetRotation(bRagdollOnGround ? NewRagdollLoc : TargetRagdollLocation, TargetRagdollRotation);
}

void AALSBaseCharacter::OnMovementModeChanged(const EMovementMode PrevMovementMode, const uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PrevMovementMode, PreviousCustomMode);

	// Use the Character Movement Mode changes to set the Movement States to the right values. This allows you to have
	// a custom set of movement states but still use the functionality of the default character movement component.

	switch (GetCharacterMovement()->MovementMode)
	{
	case MOVE_None: SetMovementState(EALSMovementState::None); break;
	case MOVE_Walking: SetMovementState(EALSMovementState::Grounded); break;
	case MOVE_NavWalking: SetMovementState(EALSMovementState::Grounded); break;
	case MOVE_Falling: SetMovementState(EALSMovementState::Freefall); break;
	case MOVE_Swimming: SetMovementState(EALSMovementState::Swimming); break;
	case MOVE_Flying: SetMovementState(EALSMovementState::Flight); break;
	case MOVE_Custom: SetMovementState(EALSMovementState::None); break;
	case MOVE_MAX: SetMovementState(EALSMovementState::None); break;
	default: SetMovementState(EALSMovementState::None); break;
	}
}

void AALSBaseCharacter::OnMovementStateChanged(const EALSMovementState PreviousState)
{
	if (MovementState == EALSMovementState::Freefall)
	{
		if (MovementAction == EALSMovementAction::None)
		{
			// If the character enters the air, set the In Air Rotation and uncrouch if crouched.
			InAirRotation = GetActorRotation();
			if (Stance == EALSStance::Crouching)
			{
				UnCrouch();
			}
		}
		else if (MovementAction == EALSMovementAction::Rolling && bRagdollOnRollfall)
		{
			// If the character is currently rolling, enable the ragdoll.
			ReplicatedRagdollStart();
		}
	}
	else if (MovementState == EALSMovementState::Ragdoll && PreviousState == EALSMovementState::Mantling)
	{
		// Stop the Mantle Timeline if transitioning to the ragdoll state while mantling.
		MantleTimeline->Stop();
	}
}

void AALSBaseCharacter::OnMovementActionChanged(const EALSMovementAction PreviousAction)
{
	// Make the character crouch if performing a roll.
	if (MovementAction == EALSMovementAction::Rolling)
	{
		Crouch();
	}

	if (PreviousAction == EALSMovementAction::Rolling)
	{
		if (DesiredStance == EALSStance::Standing)
		{
			UnCrouch();
		}
		else if (DesiredStance == EALSStance::Crouching)
		{
			Crouch();
		}
	}
}

void AALSBaseCharacter::OnStanceChanged(const EALSStance PreviousStance)
{
}

void AALSBaseCharacter::OnRotationModeChanged(const EALSRotationMode PreviousRotationMode)
{
	MainAnimInstance->RotationMode = RotationMode;
}

void AALSBaseCharacter::OnFlightModeChanged(const EALSFlightMode PreviousFlightMode)
{
	if (FlightMode == EALSFlightMode::None) // We want to stop flight.
	{
		GetCharacterMovement()->SetMovementMode(MOVE_Falling);
	}
	else if (PreviousFlightMode == EALSFlightMode::None) // We want to start flight.
	{
		GetCharacterMovement()->SetMovementMode(MOVE_Flying);
	}
	else // Changing from one flight mode to another logic:
	{
	}
}

void AALSBaseCharacter::OnGaitChanged(const EALSGait PreviousGait)
{
}

void AALSBaseCharacter::OnOverlayStateChanged(const EALSOverlayState PreviousState)
{
	MainAnimInstance->OverlayState = OverlayState;
}

void AALSBaseCharacter::OnStartCrouch(const float HalfHeightAdjust, const float ScaledHalfHeightAdjust)
{
	Super::OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	SetStance(EALSStance::Crouching);
}

void AALSBaseCharacter::OnEndCrouch(const float HalfHeightAdjust, const float ScaledHalfHeightAdjust)
{
	Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	SetStance(EALSStance::Standing);
}

void AALSBaseCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);

	if (IsLocallyControlled())
	{
		EventOnLanded();
	}
	if (HasAuthority())
	{
		Multicast_OnLanded();
	}
}

void AALSBaseCharacter::OnLandFrictionReset() const
{
	// Reset the braking friction
	GetCharacterMovement()->BrakingFrictionFactor = 0.0f;
}

void AALSBaseCharacter::SetEssentialValues(const float DeltaTime)
{
	if (GetLocalRole() != ROLE_SimulatedProxy)
	{
		ReplicatedCurrentAcceleration = GetCharacterMovement()->GetCurrentAcceleration();
		ReplicatedControlRotation = GetControlRotation();
		EasedMaxAcceleration = GetCharacterMovement()->GetMaxAcceleration();
	}

	else
	{
		EasedMaxAcceleration = GetCharacterMovement()->GetMaxAcceleration() != 0
			                       ? GetCharacterMovement()->GetMaxAcceleration()
			                       : EasedMaxAcceleration / 2;
	}

	// Interp AimingRotation to current control rotation for smooth character rotation movement. Decrease InterpSpeed
	// for slower but smoother movement.
	AimingRotation = FMath::RInterpTo(AimingRotation, ReplicatedControlRotation, DeltaTime, 30);

	// These values represent how the capsule is moving as well as how it wants to move, and therefore are essential
	// for any data driven animation system. They are also used throughout the system for various functions,
	// so I found it is easiest to manage them all in one place.

	const FVector CurrentVel = GetVelocity();

	// Set the amount of Acceleration.
	SetAcceleration((CurrentVel - PreviousVelocity) / DeltaTime);

	// Determine if the character is moving by getting it's speed. The Speed equals the length of the horizontal (x y)
	// velocity, so it does not take vertical movement into account. If the character is moving, update the last
	// velocity rotation. This value is saved because it might be useful to know the last orientation of movement
	// even after the character has stopped.
	SetSpeed(CurrentVel.Size2D());
	SetIsMoving(Speed > 1.0f);
	if (bIsMoving)
	{
		LastVelocityRotation = CurrentVel.ToOrientationRotator();
	}

	// Determine if the character has movement input by getting its movement input amount.
	// The Movement Input Amount is equal to the current acceleration divided by the max acceleration so that
	// it has a range of 0-1, 1 being the maximum possible amount of input, and 0 being none.
	// If the character has movement input, update the Last Movement Input Rotation.
	SetMovementInputAmount(ReplicatedCurrentAcceleration.Size() / EasedMaxAcceleration);
	SetHasMovementInput(MovementInputAmount > 0.0f);
	if (bHasMovementInput)
	{
		LastMovementInputRotation = ReplicatedCurrentAcceleration.ToOrientationRotator();
	}

	// Set the Aim Yaw rate by comparing the current and previous Aim Yaw value, divided by Delta Seconds.
	// This represents the speed the camera is rotating left to right.
	SetAimYawRate(FMath::Abs((AimingRotation.Yaw - PreviousAimYaw) / DeltaTime));
}

void AALSBaseCharacter::UpdateCharacterMovement(const float DeltaTime)
{
	// Set the Allowed Gait
	const EALSGait AllowedGait = GetAllowedGait();

	// Determine the Actual Gait. If it is different from the current Gait, Set the new Gait Event.
	const EALSGait ActualGait = GetActualGait(AllowedGait);

	if (ActualGait != Gait)
	{
		SetGait(ActualGait);
	}

	// Use the allowed gait to update the movement settings.
	if (bIsNetworked)
	{
		if (bForceFullNetworkedDynamicMovement)
		{
			UpdateDynamicMovementSettingsFull(DeltaTime, AllowedGait);
		}
		else
		{
			UpdateDynamicMovementSettingsNetworked(DeltaTime, AllowedGait);
		}
	}
	else
	{
		// Use curves for movement
		UpdateDynamicMovementSettingsStandalone(DeltaTime, AllowedGait);
	}
}

float AALSBaseCharacter::AdjustNewWalkingSpeed(const float DeltaTime, const float NewSpeed)
{
	float OutSpeed = NewSpeed;
	// Adjust by ground incline
	if (GetVelocity() == FVector::ZeroVector)
	{
		MovementMultiplier = 1.f;
	}
	else
	{
		const float TargetMovementMultiplier = FMath::Pow(GetCharacterMovement()->CurrentFloor.HitResult.Normal.Z, WalkingSpeedInclineBias);
		MovementMultiplier = FMath::FInterpTo(MovementMultiplier, TargetMovementMultiplier, DeltaTime, WalkingSpeedInterpRate);
		OutSpeed *= MovementMultiplier;
	}
	// Adjust by temperature and weight
	OutSpeed *= TemperatureAffect.X;
	OutSpeed *= WeightAffect.X;
	
	// Return adjusted speed
	return OutSpeed;
}

float AALSBaseCharacter::AdjustNewFlyingSpeed(const float DeltaTime, const float NewSpeed)
{
	float OutSpeed = NewSpeed;
	
	// Adjust by temperature
	OutSpeed *= TemperatureAffect.Y;
	OutSpeed *= WeightAffect.Y;

	// Return adjusted speed
	return OutSpeed;
}

float AALSBaseCharacter::AdjustNewSwimmingSpeed(const float DeltaTime, const float NewSpeed)
{
	float OutSpeed = NewSpeed;

	// Adjust by temperature
	OutSpeed *= TemperatureAffect.Z;
	OutSpeed *= WeightAffect.Z;
	
	// Return adjusted speed
	return OutSpeed;
}

void AALSBaseCharacter::UpdateFlightMovement(float DeltaTime)
{
	if (AlwaysCheckFlightConditions)
	{
		if (!CanFly())
		{
			SetFlightMode(EALSFlightMode::None);
			return;
		}
	}

	// The rest of this function calculates the auto-hover strength. This is how much downward force is generated by
	// the wings to keep the character afloat.
	
	float AutoHover;

	// Represents the strength of the wings forcing downward when flying, measured in the units below the character that
	// the pressure gradient extends.
	const float WingPressureDepth = FlightStrengthPassive / EffectiveWeight;

	FVector VelocityDirection;
	float VelocityLength;
	GetVelocity().ToDirectionAndLength(VelocityDirection, VelocityLength);

	const float VelocityAlpha = FMath::GetMappedRangeValueClamped({0, GetCharacterMovement()->MaxFlySpeed * 1.5f}, {0, 1}, VelocityLength);
	
	const FVector PressureDirection = FMath::Lerp(FVector(0, 0, -1), -VelocityDirection, VelocityAlpha);
	
	const float PressureAlpha = FlightDistanceCheck(WingPressureDepth, PressureDirection) / WingPressureDepth;

	// If a pressure curve is used, modify speed. Otherwise, default to 1 for no effect.
	float GroundPressure = 1;
	if (GroundPressureFalloff)
	{
		GroundPressure = GroundPressureFalloff->GetFloatValue(PressureAlpha);
	}

	const float LocalTemperatureAffect = TemperatureAffect.Y;
	const float LocalWeightAffect = WeightAffect.Y;

	// @TODO Design a algorythm for calculating thrust, and use it to determine lift. modify autothrust with that so that the player slowly drifts down when too heavy. 

	switch (FlightMode)
	{
	case EALSFlightMode::None: return;
	case EALSFlightMode::Neutral:
		AutoHover = (GroundPressure + 0.5) / 1.5 * LocalTemperatureAffect * LocalWeightAffect;
		break;
	case EALSFlightMode::Raising:
		AutoHover = (GroundPressure + FlightStrengthActive) * LocalTemperatureAffect * (LocalWeightAffect * 1.5);
		break;
	case EALSFlightMode::Lowering:
		AutoHover = GroundPressure * 0.5f + (-FlightStrengthActive + (LocalTemperatureAffect - 1)) + -LocalWeightAffect;
		break;
	case EALSFlightMode::Hovering:
		AutoHover = (GroundPressure + 0.5) / 1.5 * LocalTemperatureAffect * (LocalWeightAffect / 2);
		break;
	default: return;
	}
	
	const FRotator DirRotator(0.0f, AimingRotation.Yaw, 0.0f);
	AddMovementInput(UKismetMathLibrary::GetUpVector(DirRotator), AutoHover, true);
}

void AALSBaseCharacter::UpdateDynamicMovementSettingsStandalone(const float DeltaTime, const EALSGait AllowedGait)
{
	// Get the Current Movement Settings.
	CurrentMovementSettings = GetTargetMovementSettings();
	const float NewMaxSpeed = CurrentMovementSettings.GetSpeedForGait(AllowedGait);

	// Update the Acceleration, Deceleration, and Ground Friction using the Movement Curve.
	const float MappedSpeed = GetMappedSpeed();
	const FVector CurveVec = CurrentMovementSettings.MovementCurve->GetVectorValue(MappedSpeed);

	const auto CurrentMode = GetCharacterMovement()->MovementMode;
	if (CurrentMode == MOVE_Walking || CurrentMode == MOVE_NavWalking)
	{
		// Update the Character Max Walk Speed to the configured speeds based on the currently Allowed Gait.
		MyCharacterMovementComponent->SetMaxWalkingSpeed(AdjustNewWalkingSpeed(DeltaTime, NewMaxSpeed));
		GetCharacterMovement()->MaxAcceleration = CurveVec.X;
		GetCharacterMovement()->BrakingDecelerationWalking = CurveVec.Y;
		GetCharacterMovement()->GroundFriction = CurveVec.Z;
	}
	else if (CurrentMode == MOVE_Flying)
	{
		MyCharacterMovementComponent->SetMaxFlyingSpeed(AdjustNewFlyingSpeed(DeltaTime, NewMaxSpeed));
		GetCharacterMovement()->MaxAcceleration = CurveVec.X;
		GetCharacterMovement()->BrakingDecelerationFlying = CurveVec.Y;
	}
	else if (CurrentMode == MOVE_Swimming)
	{
		MyCharacterMovementComponent->SetMaxSwimmingSpeed(AdjustNewSwimmingSpeed(DeltaTime, NewMaxSpeed));
		GetCharacterMovement()->MaxAcceleration = CurveVec.X;
		GetCharacterMovement()->BrakingDecelerationSwimming = CurveVec.Y;
	}
}

void AALSBaseCharacter::UpdateDynamicMovementSettingsNetworked(const float DeltaTime, const EALSGait AllowedGait)
{
	// Get the Current Movement Settings.
	CurrentMovementSettings = GetTargetMovementSettings();
	const float NewMaxSpeed = CurrentMovementSettings.GetSpeedForGait(AllowedGait);

	const auto CurrentMode = GetCharacterMovement()->MovementMode;
	if (CurrentMode == MOVE_Walking || CurrentMode == MOVE_NavWalking)
	{
		const float NewWalkSpeed = AdjustNewWalkingSpeed(DeltaTime, NewMaxSpeed);
		// Update the Character Max Walk Speed to the configured speeds based on the currently Allowed Gait.
		if (IsLocallyControlled() || HasAuthority())
		{
			if (GetCharacterMovement()->MaxWalkSpeed != NewWalkSpeed)
			{
				MyCharacterMovementComponent->SetMaxWalkingSpeed(NewWalkSpeed);
			}
		}
		else
		{
			GetCharacterMovement()->MaxWalkSpeed = NewWalkSpeed;
		}
	}
	else if (CurrentMode == MOVE_Flying)
	{
		const float NewFlySpeed = AdjustNewFlyingSpeed(DeltaTime, NewMaxSpeed);
		// Update the Character Max Walk Speed to the configured speeds based on the currently Allowed Gait.
		if (IsLocallyControlled() || HasAuthority())
		{
			if (GetCharacterMovement()->MaxFlySpeed != NewFlySpeed)
			{
				MyCharacterMovementComponent->SetMaxFlyingSpeed(NewFlySpeed);
			}
		}
		else
		{
			GetCharacterMovement()->MaxFlySpeed = NewFlySpeed;
		}
	}
	else if (CurrentMode == MOVE_Swimming)
	{
		const float NewSwimSpeed = AdjustNewSwimmingSpeed(DeltaTime, NewMaxSpeed);
		// Update the Character Max Walk Speed to the configured speeds based on the currently Allowed Gait.
		if (IsLocallyControlled() || HasAuthority())
		{
			if (GetCharacterMovement()->MaxSwimSpeed != NewSwimSpeed)
			{
				MyCharacterMovementComponent->SetMaxSwimmingSpeed(NewSwimSpeed);
			}
		}
		else
		{
			GetCharacterMovement()->MaxSwimSpeed = NewSwimSpeed;
		}
	}
}

void AALSBaseCharacter::UpdateDynamicMovementSettingsFull(const float DeltaTime, const EALSGait AllowedGait)
{
	// Get the Current Movement Settings.
	CurrentMovementSettings = GetTargetMovementSettings();
	const float NewMaxSpeed = CurrentMovementSettings.GetSpeedForGait(AllowedGait);

	// Update the Acceleration, Deceleration, and Ground Friction using the Movement Curve.
	const float MappedSpeed = GetMappedSpeed();
	const FVector CurveVec = CurrentMovementSettings.MovementCurve->GetVectorValue(MappedSpeed);

	const auto CurrentMode = GetCharacterMovement()->MovementMode;
	if (CurrentMode == MOVE_Walking || CurrentMode == MOVE_NavWalking)
	{
		const float NewWalkSpeed = AdjustNewWalkingSpeed(DeltaTime, NewMaxSpeed);
		
		// Update the Character Max Walk Speed to the configured speeds based on the currently Allowed Gait.
		if (IsLocallyControlled() || HasAuthority())
		{
			if (GetCharacterMovement()->MaxWalkSpeed != NewWalkSpeed)
			{
				MyCharacterMovementComponent->SetMaxWalkingSpeed(NewWalkSpeed);
				GetCharacterMovement()->MaxAcceleration = CurveVec.X;
				GetCharacterMovement()->BrakingDecelerationWalking = CurveVec.Y;
				GetCharacterMovement()->GroundFriction = CurveVec.Z;
			}
		}
		else
		{
			GetCharacterMovement()->MaxWalkSpeed = NewWalkSpeed;
		}
	}
	else if (CurrentMode == MOVE_Flying)
	{
		const float NewFlySpeed = AdjustNewFlyingSpeed(DeltaTime, NewMaxSpeed);

		// Update the Character Max Walk Speed to the configured speeds based on the currently Allowed Gait.
		if (IsLocallyControlled() || HasAuthority())
		{
			if (GetCharacterMovement()->MaxFlySpeed != NewFlySpeed)
			{
				MyCharacterMovementComponent->SetMaxFlyingSpeed(NewFlySpeed);
				GetCharacterMovement()->MaxAcceleration = CurveVec.X;
				GetCharacterMovement()->BrakingDecelerationFlying = CurveVec.Y;
			}
		}
		else
		{
			GetCharacterMovement()->MaxFlySpeed = NewFlySpeed;
		}
	}
	else if (CurrentMode == MOVE_Swimming)
	{
		const float NewSwimSpeed = AdjustNewSwimmingSpeed(DeltaTime, NewMaxSpeed);

		// Update the Character Max Walk Speed to the configured speeds based on the currently Allowed Gait.
		if (IsLocallyControlled() || HasAuthority())
		{
			if (GetCharacterMovement()->MaxSwimSpeed != NewSwimSpeed)
			{
				MyCharacterMovementComponent->SetMaxSwimmingSpeed(NewSwimSpeed);
				GetCharacterMovement()->MaxAcceleration = CurveVec.X;
				GetCharacterMovement()->BrakingDecelerationSwimming = CurveVec.Y;
			}
		}
		else
		{
			GetCharacterMovement()->MaxSwimSpeed = NewSwimSpeed;
		}
	}
}

void AALSBaseCharacter::UpdateGroundedRotation(const float DeltaTime)
{
	if (MovementAction == EALSMovementAction::None)
	{
		const bool bCanUpdateMovingRot = ((bIsMoving && bHasMovementInput) || Speed > 150.0f) && !HasAnyRootMotion();
		if (bCanUpdateMovingRot)
		{
			const float GroundedRotationRate = CalculateGroundedRotationRate();
			if (RotationMode == EALSRotationMode::VelocityDirection)
			{
				// Velocity Direction Rotation
				SmoothCharacterRotation({0.0f, LastVelocityRotation.Yaw, 0.0f}, 800.0f, GroundedRotationRate, DeltaTime);
			}
			else if (RotationMode == EALSRotationMode::LookingDirection)
			{
				// Looking Direction Rotation
				float YawValue;
				if (Gait == EALSGait::GaitFast)
				{
					YawValue = LastVelocityRotation.Yaw;
				}
				else
				{
					// Walking or Running..
					const float YawOffsetCurveVal = MainAnimInstance->GetCurveValue(FName(TEXT("YawOffset")));
					YawValue = AimingRotation.Yaw + YawOffsetCurveVal;
				}
				SmoothCharacterRotation({0.0f, YawValue, 0.0f}, 500.0f, GroundedRotationRate, DeltaTime);
			}
			else if (RotationMode == EALSRotationMode::Aiming)
			{
				const float ControlYaw = AimingRotation.Yaw;
				SmoothCharacterRotation({0.0f, ControlYaw, 0.0f}, 1000.0f, 20.0f, DeltaTime);
			}
		}
		else
		{
			// Not Moving

			if (RestrictAiming)
			{
				LimitRotation(-100.0f, 100.0f, 20.0f, DeltaTime);
			}

			// Apply the RotationAmount curve from Turn In Place Animations.
			// The Rotation Amount curve defines how much rotation should be applied each frame,
			// and is calculated for animations that are animated at 30fps.

			const float RotAmountCurve = MainAnimInstance->GetCurveValue(FName(TEXT("RotationAmount")));

			if (FMath::Abs(RotAmountCurve) > 0.001f)
			{
				if (GetLocalRole() == ROLE_AutonomousProxy)
				{
					TargetRotation.Yaw = UKismetMathLibrary::NormalizeAxis(
						TargetRotation.Yaw + (RotAmountCurve * (DeltaTime / (1.0f / 30.0f))));
					SetActorRotation(TargetRotation);
				}
				else
				{
					AddActorWorldRotation({0, RotAmountCurve * (DeltaTime / (1.0f / 30.0f)), 0});
				}
				TargetRotation = GetActorRotation();
			}
		}
	}
	else if (MovementAction == EALSMovementAction::Rolling)
	{
		// Rolling Rotation

		if (bHasMovementInput)
		{
			SmoothCharacterRotation({0.0f, LastMovementInputRotation.Yaw, 0.0f}, 0.0f, 2.0f, DeltaTime);
		}
	}
}

void AALSBaseCharacter::UpdateFallingRotation(const float DeltaTime)
{
	if (RotationMode == EALSRotationMode::VelocityDirection || RotationMode == EALSRotationMode::LookingDirection)
	{
		// Velocity / Looking Direction Rotation
		SmoothCharacterRotation({0.0f, InAirRotation.Yaw, 0.0f}, 0.0f, 5.0f, DeltaTime);
	}
	else if (RotationMode == EALSRotationMode::Aiming)
	{
		// Aiming Rotation
		SmoothCharacterRotation({0.0f, AimingRotation.Yaw, 0.0f}, 0.0f, 15.0f, DeltaTime);
		InAirRotation = GetActorRotation();
	}
}

void AALSBaseCharacter::UpdateFlightRotation(const float DeltaTime)
{
	const float SpeedCache = GetMappedSpeed();
	const float CheckAltitude = SpeedCache * 100.f;

	// Map distant to ground to a unit scaler.
	const float Alpha_Altitude = FMath::GetMappedRangeValueClamped({0.f, CheckAltitude},
															       {0.f, 1.f},
															       RelativeAltitude);
	
	// Combine unit scalers equal to smaller.
	const float RotationAlpha = Alpha_Altitude * (SpeedCache / 3);

	// Calculate input leaning.
	const FVector Lean = GetInputAcceleration() * MaxLean * RotationAlpha;

	const float Pitch = FMath::FInterpTo(GetActorRotation().Pitch, Lean.X * -1, DeltaTime, MaxFlightRotationRate);
	const float Roll = FMath::FInterpTo(GetActorRotation().Roll, Lean.Y, DeltaTime, MaxFlightRotationRate);

	if (bHasMovementInput)
	{
		if (RotationMode == EALSRotationMode::VelocityDirection || RotationMode == EALSRotationMode::LookingDirection)
		{
			// Velocity / Looking Direction Rotation
			const float InterpSpeed = FMath::GetMappedRangeValueClamped({0, 3}, {0.1, MaxFlightRotationRate}, SpeedCache);
			SmoothCharacterRotation({Pitch, AimingRotation.Yaw, Roll}, 0.0f, InterpSpeed, DeltaTime);
		}
		else if (RotationMode == EALSRotationMode::Aiming)
		{
			// Aiming Rotation
			SmoothCharacterRotation({Pitch, AimingRotation.Yaw, Roll / 2}, 0.0f, MaxFlightRotationRate, DeltaTime);
		}
	}
	else
	{
		SmoothCharacterRotation({0, GetActorRotation().Yaw, 0}, 0.0f, MaxFlightRotationRate, DeltaTime);
	}
	InAirRotation = GetActorRotation();
}

void AALSBaseCharacter::UpdateSwimmingRotation(const float DeltaTime)
{
	float const Lean = FMath::GetMappedRangeValueUnclamped({0, 3}, {0, 90}, GetMappedSpeed());

	SmoothCharacterRotation({Lean * -GetInputAcceleration().X, AimingRotation.Yaw, 0.0f}, 0.f, 2.5f, DeltaTime);
}

bool AALSBaseCharacter::CanFly()
{
	return FlightCheck()
			&& bFlightEnabled
			&& FMath::IsWithin(Temperature, FlightTempBounds.X, FlightTempBounds.Y)
			&& EffectiveWeight < FlightWeightCutOff;
}

bool AALSBaseCharacter::FlightInterruptThresholdCheck() const
{
	float MyVelLen;
	FVector MyVelDir;
	GetVelocity().GetAbs().ToDirectionAndLength(MyVelDir, MyVelLen);
	return MyVelLen >= FlightInterruptThreshold;
}

void AALSBaseCharacter::MantleStart(const float MantleHeight, const FALSComponentAndTransform& MantleLedgeWS, const EALSMantleType MantleType)
{
	// Step 1: Get the Mantle Asset and use it to set the new Mantle Params.
	const FALSMantleAsset& MantleAsset = GetMantleAsset(MantleType);

	MantleParams.AnimMontage = MantleAsset.AnimMontage;
	MantleParams.PositionCorrectionCurve = MantleAsset.PositionCorrectionCurve;
	MantleParams.StartingOffset = MantleAsset.StartingOffset;
	MantleParams.StartingPosition = FMath::GetMappedRangeValueClamped({MantleAsset.LowHeight, MantleAsset.HighHeight},
	                                                                  {MantleAsset.LowStartPosition, MantleAsset.HighStartPosition},
	                                                                  MantleHeight);
	MantleParams.PlayRate = FMath::GetMappedRangeValueClamped({MantleAsset.LowHeight, MantleAsset.HighHeight},
	                                                          {MantleAsset.LowPlayRate, MantleAsset.HighPlayRate}, MantleHeight);

	// Step 2: Convert the world space target to the mantle component's local space for use in moving objects.
	MantleLedgeLS.Component = MantleLedgeWS.Component;
	MantleLedgeLS.Transform = MantleLedgeWS.Transform * MantleLedgeWS.Component->GetComponentToWorld().Inverse();

	// Step 3: Set the Mantle Target and calculate the Starting Offset
	// (offset amount between the actor and target transform).
	MantleTarget = MantleLedgeWS.Transform;
	MantleActualStartOffset = UALSMathLibrary::TransfromSub(GetActorTransform(), MantleTarget);

	// Step 4: Calculate the Animated Start Offset from the Target Location.
	// This would be the location the actual animation starts at relative to the Target Transform.
	FVector RotatedVector = MantleTarget.GetRotation().Vector() * MantleParams.StartingOffset.Y;
	RotatedVector.Z = MantleParams.StartingOffset.Z;
	const FTransform StartOffset(MantleTarget.Rotator(), MantleTarget.GetLocation() - RotatedVector,
	                             FVector::OneVector);
	MantleAnimatedStartOffset = UALSMathLibrary::TransfromSub(StartOffset, MantleTarget);

	// Step 5: Clear the Character Movement Mode and set the Movement State to Mantling
	GetCharacterMovement()->SetMovementMode(MOVE_None);
	SetMovementState(EALSMovementState::Mantling);

	// Step 6: Configure the Mantle Timeline so that it is the same length as the
	// Lerp/Correction curve minus the starting position, and plays at the same speed as the animation.
	// Then start the timeline.
	float MinTime = 0.0f;
	float MaxTime = 0.0f;
	MantleParams.PositionCorrectionCurve->GetTimeRange(MinTime, MaxTime);
	MantleTimeline->SetTimelineLength(MaxTime - MantleParams.StartingPosition);
	MantleTimeline->SetPlayRate(MantleParams.PlayRate);
	MantleTimeline->PlayFromStart();

	// Step 7: Play the Anim Montaget if valid.
	if (IsValid(MantleParams.AnimMontage))
	{
		MainAnimInstance->Montage_Play(MantleParams.AnimMontage, MantleParams.PlayRate,
		                               EMontagePlayReturnType::MontageLength, MantleParams.StartingPosition, false);
	}
}

bool AALSBaseCharacter::MantleCheck(const FALSMantleTraceSettings& TraceSettings)
{
	// Step 1: Trace forward to find a wall / object the character cannot walk on.
	const FVector& CapsuleBaseLocation = UALSMathLibrary::GetCapsuleBaseLocation(2.0f, GetCapsuleComponent());
	FVector TraceStart = CapsuleBaseLocation + GetMovementDirection() * -30.0f;
	TraceStart.Z += (TraceSettings.MaxLedgeHeight + TraceSettings.MinLedgeHeight) / 2.0f;
	FVector TraceEnd = TraceStart + GetMovementDirection() * TraceSettings.ReachDistance;
	
	const float HalfHeight = 1.0f + ((TraceSettings.MaxLedgeHeight - TraceSettings.MinLedgeHeight) / 2.0f);

	UWorld* World = GetWorld();
	check(World);

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);

	DrawDebugLine(World, TraceStart, TraceEnd, FColor::Red, false, 1.f, 0, 3);
	
	FHitResult HitResult;
	World->SweepSingleByChannel(HitResult, TraceStart, TraceEnd, FQuat::Identity, UALS_Settings::Get()->MantleCheckChannel,
	                            FCollisionShape::MakeCapsule(TraceSettings.ForwardTraceRadius, HalfHeight), Params);

	if (!HitResult.IsValidBlockingHit() || GetCharacterMovement()->IsWalkable(HitResult)) return false; // Not a valid surface to mantle

	const FVector InitialTraceImpactPoint = HitResult.ImpactPoint;
	const FVector InitialTraceNormal = HitResult.ImpactNormal;

	// Step 2: Trace downward from the first trace's Impact Point and determine if the hit location is walkable.
	FVector DownwardTraceEnd = InitialTraceImpactPoint;
	DownwardTraceEnd.Z = CapsuleBaseLocation.Z;
	DownwardTraceEnd += InitialTraceNormal * -15.0f;
	FVector DownwardTraceStart = DownwardTraceEnd;
	DownwardTraceStart.Z += TraceSettings.MaxLedgeHeight + TraceSettings.DownwardTraceRadius + 1.0f;

	DrawDebugLine(World, DownwardTraceStart, DownwardTraceEnd, FColor::Blue, false, 1.f, 0, 3);

	World->SweepSingleByChannel(HitResult, DownwardTraceStart, DownwardTraceEnd, FQuat::Identity,
	                            UALS_Settings::Get()->MantleCheckChannel, FCollisionShape::MakeSphere(TraceSettings.DownwardTraceRadius), Params);


	if (!GetCharacterMovement()->IsWalkable(HitResult))
	{
		// Not a valid surface to mantle
		return false;
	}

	const FVector DownTraceLocation(HitResult.Location.X, HitResult.Location.Y, HitResult.ImpactPoint.Z);
	UPrimitiveComponent* HitComponent = HitResult.GetComponent();

	// Step 3: Check if the capsule has room to stand at the downward trace's location.
	// If so, set that location as the Target Transform and calculate the mantle height.
	const FVector& CapsuleLocationFBase = UALSMathLibrary::GetCapsuleLocationFromBase(DownTraceLocation, 2.0f, GetCapsuleComponent());
	const bool bCapsuleHasRoom = UALSMathLibrary::CapsuleHasRoomCheck(GetCapsuleComponent(), CapsuleLocationFBase, 0.0f,
	                                                                  0.0f);

	if (!bCapsuleHasRoom) return false; // Capsule doesn't have enough room to mantle

	const FTransform TargetTransform(
		(InitialTraceNormal * FVector(-1.0f, -1.0f, 0.0f)).ToOrientationRotator(),
		CapsuleLocationFBase,
		FVector::OneVector);

	const float MantleHeight = (CapsuleLocationFBase - GetActorLocation()).Z;

	// Step 4: Determine the Mantle Type by checking the movement mode and Mantle Height.
	EALSMantleType MantleType;
	if (MovementState == EALSMovementState::Freefall)
	{
		MantleType = EALSMantleType::FallingCatch;
	}
	else
	{
		MantleType = MantleHeight > 125.0f ? EALSMantleType::HighMantle : EALSMantleType::LowMantle;
	}

	// Step 4.5: Check if gameplay will allow the chosen Mantle type
	if (!CanMantle(MantleType)) return false;

	// Step 5: If everything checks out, start the Mantle
	FALSComponentAndTransform MantleWS;
	MantleWS.Component = HitComponent;
	MantleWS.Transform = TargetTransform;
	MantleStart(MantleHeight, MantleWS, MantleType);
	Server_MantleStart(MantleHeight, MantleWS, MantleType);

	return true;
}

void AALSBaseCharacter::MantleUpdate(const float BlendIn)
{
	// Step 1: Continually update the mantle target from the stored local transform to follow along with moving objects
	MantleTarget = UALSMathLibrary::MantleComponentLocalToWorld(MantleLedgeLS);

	// Step 2: Update the Position and Correction Alphas using the Position/Correction curve set for each Mantle.
	const FVector CurveVec = MantleParams.PositionCorrectionCurve
	                                     ->GetVectorValue(MantleParams.StartingPosition + MantleTimeline->GetPlaybackPosition());
	const float PositionAlpha = CurveVec.X;
	const float XYCorrectionAlpha = CurveVec.Y;
	const float ZCorrectionAlpha = CurveVec.Z;

	// Step 3: Lerp multiple transforms together for independent control over the horizontal
	// and vertical blend to the animated start position, as well as the target position.

	// Blend into the animated horizontal and rotation offset using the Y value of the Position/Correction Curve.
	const FTransform TargetHzTransform(MantleAnimatedStartOffset.GetRotation(),
	                                   {
		                                   MantleAnimatedStartOffset.GetLocation().X, MantleAnimatedStartOffset.GetLocation().Y,
		                                   MantleActualStartOffset.GetLocation().Z
	                                   },
	                                   FVector::OneVector);
	const FTransform& HzLerpResult =
		UKismetMathLibrary::TLerp(MantleActualStartOffset, TargetHzTransform, XYCorrectionAlpha);

	// Blend into the animated vertical offset using the Z value of the Position/Correction Curve.
	const FTransform TargetVtTransform(MantleActualStartOffset.GetRotation(),
	                                   {
		                                   MantleActualStartOffset.GetLocation().X, MantleActualStartOffset.GetLocation().Y,
		                                   MantleAnimatedStartOffset.GetLocation().Z
	                                   },
	                                   FVector::OneVector);
	const FTransform& VtLerpResult =
		UKismetMathLibrary::TLerp(MantleActualStartOffset, TargetVtTransform, ZCorrectionAlpha);

	const FTransform ResultTransform(HzLerpResult.GetRotation(),
	                                 {HzLerpResult.GetLocation().X, HzLerpResult.GetLocation().Y, VtLerpResult.GetLocation().Z},
	                                 FVector::OneVector);

	// Blend from the currently blending transforms into the final mantle target using the X
	// value of the Position/Correction Curve.
	const FTransform& ResultLerp = UKismetMathLibrary::TLerp(UALSMathLibrary::TransfromAdd(MantleTarget, ResultTransform), MantleTarget,
	                                                         PositionAlpha);

	// Initial Blend In (controlled in the timeline curve) to allow the actor to blend into the Position/Correction
	// curve at the midoint. This prevents pops when mantling an object lower than the animated mantle.
	const FTransform& LerpedTarget =
		UKismetMathLibrary::TLerp(UALSMathLibrary::TransfromAdd(MantleTarget, MantleActualStartOffset), ResultLerp, BlendIn);

	// Step 4: Set the actors location and rotation to the Lerped Target.
	SetActorLocationAndTargetRotation(LerpedTarget.GetLocation(), LerpedTarget.GetRotation().Rotator());
}

void AALSBaseCharacter::MantleEnd()
{
	// Set the Character Movement Mode to Walking
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
}

float AALSBaseCharacter::GetMappedSpeed() const
{
	// Map the character's current speed to the configured movement speeds with a range of 0-3,
	// with 0 = stopped, 1 = the Slow Speed, 2 = the Normal Speed, and 3 = the Fast Speed.
	// This allows us to vary the movement speeds but still use the mapped range in calculations for consistent results

	const float LocSlowSpeed = CurrentMovementSettings.SlowSpeed;
	const float LocNormalSpeed = CurrentMovementSettings.NormalSpeed;
	const float LocFastSpeed = CurrentMovementSettings.FastSpeed;

	if (Speed > LocNormalSpeed)
	{
		return FMath::GetMappedRangeValueClamped({LocNormalSpeed, LocFastSpeed}, {2.0f, 3.0f}, Speed);
	}

	if (Speed > LocSlowSpeed)
	{
		return FMath::GetMappedRangeValueClamped({LocSlowSpeed, LocNormalSpeed}, {1.0f, 2.0f}, Speed);
	}

	return FMath::GetMappedRangeValueClamped({0.0f, LocSlowSpeed}, {0.0f, 1.0f}, Speed);
}

EALSGait AALSBaseCharacter::GetAllowedGait() const
{
	// Calculate the Allowed Gait. This represents the maximum Gait the character is currently allowed to be in,
	// and can be determined by the desired gait, the rotation mode, the stance, etc. For example,
	// if you wanted to force the character into a walking state while indoors, this could be done here.

	if (Stance == EALSStance::Standing)
	{
		if (RotationMode != EALSRotationMode::Aiming)
		{
			if (DesiredGait == EALSGait::GaitFast)
			{
				return CanSprint() ? EALSGait::GaitFast : EALSGait::GaitNormal;
			}
			return DesiredGait;
		}
	}

	// Crouching stance & Aiming rot mode has same behaviour

	if (DesiredGait == EALSGait::GaitFast)
	{
		return EALSGait::GaitNormal;
	}

	return DesiredGait;
}

EALSGait AALSBaseCharacter::GetActualGait(const EALSGait AllowedGait) const
{
	// Get the Actual Gait. This is calculated by the actual movement of the character,  and so it can be different
	// from the desired gait or allowed gait. For instance, if the Allowed Gait becomes walking,
	// the Actual gait will still be running untill the character decelerates to the walking speed.

	const float LocWalkSpeed = CurrentMovementSettings.SlowSpeed;
	const float LocRunSpeed = CurrentMovementSettings.NormalSpeed;

	if (Speed > LocRunSpeed + 10.0f)
	{
		if (AllowedGait == EALSGait::GaitFast)
		{
			return EALSGait::GaitFast;
		}
		return EALSGait::GaitNormal;
	}

	if (Speed >= LocWalkSpeed + 10.0f)
	{
		return EALSGait::GaitNormal;
	}

	return EALSGait::GaitSlow;
}

void AALSBaseCharacter::SmoothCharacterRotation(const FRotator Target,
												const float TargetInterpSpeed,
												const float ActorInterpSpeed,
                                                const float DeltaTime)
{
	// Interpolate the Target Rotation for extra smooth rotation behavior
	TargetRotation =
		FMath::RInterpConstantTo(TargetRotation, Target, DeltaTime, TargetInterpSpeed);
	SetActorRotation(
		FMath::RInterpTo(GetActorRotation(), TargetRotation, DeltaTime, ActorInterpSpeed));
}

float AALSBaseCharacter::CalculateGroundedRotationRate() const
{
	// Calculate the rotation rate by using the current Rotation Rate Curve in the Movement Settings.
	// Using the curve in conjunction with the mapped speed gives you a high level of control over the rotation
	// rates for each speed. Increase the speed if the camera is rotating quickly for more responsive rotation.

	const float MappedSpeedVal = GetMappedSpeed();
	const float CurveVal =
		CurrentMovementSettings.RotationRateCurve->GetFloatValue(MappedSpeedVal);
	const float ClampedAimYawRate = FMath::GetMappedRangeValueClamped({0.0f, 300.0f}, {1.0f, 3.0f}, AimYawRate);
	return CurveVal * ClampedAimYawRate;
}

float AALSBaseCharacter::CalculateFlightRotationRate() const
{
	// Calculate the rotation rate by using the current Rotation Rate Curve in the Movement Settings.
	// Using the curve in conjunction with the mapped speed gives you a high level of control over the rotation
	// rates for each speed. Increase the speed if the camera is rotating quickly for more responsive rotation.

	const float MappedSpeedVal = GetMappedSpeed();
	const float CurveVal =
        CurrentMovementSettings.RotationRateCurve->GetFloatValue(MappedSpeedVal);
	const float ClampedAimYawRate = FMath::GetMappedRangeValueClamped({0.0f, 300.0f}, {1.0f, 3.0f}, AimYawRate);
	return CurveVal * ClampedAimYawRate;
}

void AALSBaseCharacter::UpdateRelativeAltitude()
{
	RelativeAltitude = FlightDistanceCheck(TroposphereHeight, FVector::DownVector);
}

float AALSBaseCharacter::FlightDistanceCheck(const float CheckDistance, const FVector Direction) const
{
	UWorld* World = GetWorld();
	if (!World) return 0.f;
	
	FHitResult HitResult;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);

	const FVector CheckStart = GetActorLocation() - FVector{0, 0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight()};
	const FVector CheckEnd = CheckStart + (Direction * CheckDistance);
	World->LineTraceSingleByChannel(HitResult, CheckStart, CheckEnd, UALS_Settings::Get()->FlightCheckChannel, Params);

	DrawDebugLine(World, CheckStart, CheckEnd, FColor::Silver, false, 0.5f, 0, 2);
	
	if (HitResult.bBlockingHit)
	{
		return HitResult.Distance;
	}
	return CheckDistance;
}

void AALSBaseCharacter::LimitRotation(const float AimYawMin, const float AimYawMax, const float InterpSpeed, const float DeltaTime)
{
	// Prevent the character from rotating past a certain angle.
	FRotator Delta = AimingRotation - GetActorRotation();
	Delta.Normalize();
	const float RangeVal = Delta.Yaw;

	if (RangeVal < AimYawMin || RangeVal > AimYawMax)
	{
		const float ControlRotYaw = AimingRotation.Yaw;
		const float TargetYaw = ControlRotYaw + (RangeVal > 0.0f ? AimYawMin : AimYawMax);
		SmoothCharacterRotation({0.0f, TargetYaw, 0.0f}, 0.0f, InterpSpeed, DeltaTime);
	}
}

//**		WORLD INTERACTION			**//

void AALSBaseCharacter::SetTemperature(const float NewTemperature)
{
	Temperature = NewTemperature;
	if (TemperatureAffectCurve)
	{
		TemperatureAffect = TemperatureAffectCurve->GetVectorValue(Temperature);
	}
	else
	{
		TemperatureAffect = {1, 1, 1};
	}
}

void AALSBaseCharacter::SetWeight(const float NewWeight)
{
	EffectiveWeight = NewWeight;
	if (WeightAffectCurve)
	{
		WeightAffect = WeightAffectCurve->GetVectorValue(EffectiveWeight / WeightAffectScale);
	}
	else
	{
		WeightAffect = {1, 1, 1};
	}
}

//**		VARIABLE REPLICATION		**//

void AALSBaseCharacter::OnRep_RotationMode(const EALSRotationMode PrevRotMode)
{
	OnRotationModeChanged(PrevRotMode);
}

void AALSBaseCharacter::OnRep_FlightMode(const EALSFlightMode PrevFlightMode)
{
	OnFlightModeChanged(PrevFlightMode);
}

void AALSBaseCharacter::OnRep_OverlayState(const EALSOverlayState PrevOverlayState)
{
	OnOverlayStateChanged(PrevOverlayState);
}

//**		FUNCTION REPLICATION		**//

void AALSBaseCharacter::OnBreakfall_Implementation()
{
	Replicated_PlayMontage(GetRollAnimation(), 1.35);
}

void AALSBaseCharacter::Replicated_PlayMontage_Implementation(UAnimMontage* Montage, const float Track)
{
	// Roll: Simply play a Root Motion Montage.
	MainAnimInstance->Montage_Play(Montage, Track);
	Server_PlayMontage(Montage, Track);
}

void AALSBaseCharacter::ReplicatedRagdollStart()
{
	if (HasAuthority())
	{
		Multicast_RagdollStart();
	}
	else
	{
		Server_RagdollStart();
	}
}

void AALSBaseCharacter::ReplicatedRagdollEnd()
{
	if (HasAuthority())
	{
		Multicast_RagdollEnd(GetActorLocation());
	}
	else
	{
		Server_RagdollEnd(GetActorLocation());
	}
}

void AALSBaseCharacter::Server_SetMeshLocationDuringRagdoll_Implementation(const FVector MeshLocation)
{
	TargetRagdollLocation = MeshLocation;
}

void AALSBaseCharacter::Server_SetDesiredStance_Implementation(const EALSStance NewStance)
{
	SetDesiredStance(NewStance);
}

void AALSBaseCharacter::Server_SetDesiredGait_Implementation(const EALSGait NewGait)
{
	SetDesiredGait(NewGait);
}

void AALSBaseCharacter::Server_SetDesiredRotationMode_Implementation(const EALSRotationMode NewRotMode)
{
	SetDesiredRotationMode(NewRotMode);
}

void AALSBaseCharacter::Server_SetRotationMode_Implementation(const EALSRotationMode NewRotationMode)
{
	SetRotationMode(NewRotationMode);
}

void AALSBaseCharacter::Server_SetFlightMode_Implementation(const EALSFlightMode NewFlightMode)
{
	SetFlightMode(NewFlightMode);
}

void AALSBaseCharacter::Server_SetOverlayState_Implementation(const EALSOverlayState NewState)
{
	SetOverlayState(NewState);
}

void AALSBaseCharacter::Multicast_OnLanded_Implementation()
{
	if (!IsLocallyControlled())
	{
		EventOnLanded();
	}
}

void AALSBaseCharacter::Server_MantleStart_Implementation(const float MantleHeight,
                                                          const FALSComponentAndTransform& MantleLedgeWS,
                                                          const EALSMantleType MantleType)
{
	Multicast_MantleStart(MantleHeight, MantleLedgeWS, MantleType);
}

void AALSBaseCharacter::Multicast_MantleStart_Implementation(const float MantleHeight,
                                                             const FALSComponentAndTransform& MantleLedgeWS,
                                                             const EALSMantleType MantleType)
{
	if (!IsLocallyControlled())
	{
		MantleStart(MantleHeight, MantleLedgeWS, MantleType);
	}
}

void AALSBaseCharacter::Server_PlayMontage_Implementation(UAnimMontage* Montage, const float Track)
{
	Multicast_PlayMontage(Montage, Track);
}

void AALSBaseCharacter::Multicast_PlayMontage_Implementation(UAnimMontage* Montage, const float Track)
{
	if (!IsLocallyControlled())
	{
		// Roll: Simply play a Root Motion Montage.
		MainAnimInstance->Montage_Play(Montage, Track);
	}
}

void AALSBaseCharacter::OnJumped_Implementation()
{
	Super::OnJumped_Implementation();
	if (IsLocallyControlled())
	{
		EventOnJumped();
	}
	if (HasAuthority())
	{
		Multicast_OnJumped();
	}
}

void AALSBaseCharacter::Multicast_OnJumped_Implementation()
{
	if (!IsLocallyControlled())
	{
		EventOnJumped();
	}
}

void AALSBaseCharacter::Server_RagdollStart_Implementation()
{
	Multicast_RagdollStart();
}

void AALSBaseCharacter::Multicast_RagdollStart_Implementation()
{
	RagdollStart();
}

void AALSBaseCharacter::Server_RagdollEnd_Implementation(const FVector CharacterLocation)
{
	Multicast_RagdollEnd(CharacterLocation);
}

void AALSBaseCharacter::Multicast_RagdollEnd_Implementation(const FVector CharacterLocation)
{
	RagdollEnd();
}