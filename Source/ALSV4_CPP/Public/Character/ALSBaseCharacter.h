// Project:         Advanced Locomotion System V4 on C++
// Copyright:       Copyright (C) 2020 Doğa Can Yanıkoğlu
// License:         MIT License (http://www.opensource.org/licenses/mit-license.php)
// Source Code:     https://github.com/dyanikoglu/ALSV4_CPP
// Original Author: Doğa Can Yanıkoğlu
// Contributors:    Haziq Fadhil

#pragma once

#include "CoreMinimal.h"
#include "Components/TimelineComponent.h"
#include "Library/ALSCharacterEnumLibrary.h"
#include "Library/ALSCharacterStructLibrary.h"
#include "Engine/DataTable.h"
#include "GameFramework/Character.h"
#include "ALSBaseCharacter.generated.h"

class UTimelineComponent;
class UAnimInstance;
class UAnimMontage;
class UALSCharacterAnimInstance;
enum class EVisibilityBasedAnimTickOption : uint8;

/*
 * Base character class
 */
UCLASS(Abstract)
class ALSV4_CPP_API AALSBaseCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AALSBaseCharacter(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category= "Movement")
	FORCEINLINE class UALSCharacterMovementComponent* GetMyMovementComponent() const
	{
		return MyCharacterMovementComponent;
	}

	virtual void Tick(float DeltaTime) override;

	virtual void BeginPlay() override;

	virtual void PreInitializeComponents() override;

	virtual void PostInitializeComponents() override;

	virtual void NotifyHit(UPrimitiveComponent* MyComp, AActor* Other, UPrimitiveComponent* OtherComp, bool bSelfMoved,
						   FVector HitLocation, FVector HitNormal, FVector NormalImpulse,
						   const FHitResult& Hit) override;

	// We are overriding this to implement custom handling for flight logic.
	virtual void AddMovementInput(FVector WorldDirection, float ScaleValue, bool bForce = false) override;

	/** Ragdoll System */

	/** Implement on BP to get required get up animation according to character's state */
	UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, Category = "ALS|Ragdoll System")
	UAnimMontage* GetGetUpAnimation(bool bRagdollFaceUpState);

	UFUNCTION(BlueprintCallable, Category = "ALS|Ragdoll System")
	virtual void RagdollStart();

	UFUNCTION(BlueprintCallable, Category = "ALS|Ragdoll System")
	virtual void RagdollEnd();

	UFUNCTION(BlueprintCallable, Server, Unreliable, Category = "ALS|Ragdoll System")
	void Server_SetMeshLocationDuringRagdoll(FVector MeshLocation);

	/** Character States */

	UFUNCTION(BlueprintCallable, Category = "ALS|Character States")
	void SetMovementState(EALSMovementState NewState);

	UFUNCTION(BlueprintGetter, Category = "ALS|Character States")
	EALSMovementState GetMovementState() const { return MovementState; }

	UFUNCTION(BlueprintGetter, Category = "ALS|Character States")
	EALSMovementState GetPrevMovementState() const { return PrevMovementState; }

	UFUNCTION(BlueprintCallable, Category = "ALS|Character States")
	void SetMovementAction(EALSMovementAction NewAction);

	UFUNCTION(BlueprintGetter, Category = "ALS|Character States")
	EALSMovementAction GetMovementAction() const { return MovementAction; }

	UFUNCTION(BlueprintCallable, Category = "ALS|Character States")
	void SetStance(EALSStance NewStance);

	UFUNCTION(BlueprintGetter, Category = "ALS|Character States")
	EALSStance GetStance() const { return Stance; }

	UFUNCTION(BlueprintCallable, Category = "ALS|Character States")
	void SetGait(EALSGait NewGait);

	UFUNCTION(BlueprintGetter, Category = "ALS|Character States")
	EALSGait GetGait() const { return Gait; }

	UFUNCTION(BlueprintGetter, Category = "ALS|CharacterStates")
	EALSGait GetDesiredGait() const { return DesiredGait; }

	UFUNCTION(BlueprintCallable, Category = "ALS|Character States")
	void SetRotationMode(EALSRotationMode NewRotationMode);

	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "ALS|Character States")
	void Server_SetRotationMode(EALSRotationMode NewRotationMode);

	UFUNCTION(BlueprintCallable, Category = "ALS|Input")
	void SetFlightMode(EALSFlightMode NewFlightMode);

	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "ALS|Character States")
	void Server_SetFlightMode(EALSFlightMode NewFlightMode);

	UFUNCTION(BlueprintGetter, Category = "ALS|Character States")
	EALSRotationMode GetRotationMode() const { return RotationMode; }

	UFUNCTION(BlueprintCallable, Category = "ALS|Character States")
	void SetOverlayState(EALSOverlayState NewState);

	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "ALS|Character States")
	void Server_SetOverlayState(EALSOverlayState NewState);

	UFUNCTION(BlueprintGetter, Category = "ALS|Character States")
	EALSOverlayState GetOverlayState() const { return OverlayState; }

	UFUNCTION(BlueprintGetter, Category = "ALS|Character States")
	EALSOverlayState SwitchRight() const { return OverlayState; }

	/** Landed, Jumped, Rolling, Mantling and Ragdoll*/

	/** On Landed*/
	UFUNCTION(BlueprintCallable, Category = "ALS|Character States")
	void EventOnLanded();

	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = "ALS|Character States")
	void Multicast_OnLanded();

	/** On Jumped*/
	UFUNCTION(BlueprintCallable, Category = "ALS|Character States")
	void EventOnJumped();

	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = "ALS|Character States")
	void Multicast_OnJumped();

	/** Rolling Montage Play Replication*/
	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "ALS|Character States")
	void Server_PlayMontage(UAnimMontage* Montage, float Track);

	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = "ALS|Character States")
	void Multicast_PlayMontage(UAnimMontage* Montage, float Track);

	/** Mantling*/
	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "ALS|Character States")
	void Server_MantleStart(float MantleHeight, const FALSComponentAndTransform& MantleLedgeWS,
							EALSMantleType MantleType);

	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = "ALS|Character States")
	void Multicast_MantleStart(float MantleHeight, const FALSComponentAndTransform& MantleLedgeWS,
							   EALSMantleType MantleType);

	/** Ragdolling*/
	UFUNCTION(BlueprintCallable, Category = "ALS|Character States")
	void ReplicatedRagdollStart();

	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "ALS|Character States")
	void Server_RagdollStart();

	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = "ALS|Character States")
	void Multicast_RagdollStart();

	UFUNCTION(BlueprintCallable, Category = "ALS|Character States")
	void ReplicatedRagdollEnd();

	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "ALS|Character States")
	void Server_RagdollEnd(FVector CharacterLocation);

	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = "ALS|Character States")
	void Multicast_RagdollEnd(FVector CharacterLocation);

	/** Input */

	UFUNCTION(BlueprintGetter, Category = "ALS|Input")
	EALSStance GetDesiredStance() const { return DesiredStance; }

	UFUNCTION(BlueprintSetter, Category = "ALS|Input")
	void SetDesiredStance(EALSStance NewStance);

	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "ALS|Input")
	void Server_SetDesiredStance(EALSStance NewStance);

	UFUNCTION(BlueprintCallable, Category = "ALS|Character States")
	void SetDesiredGait(EALSGait NewGait);

	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "ALS|Character States")
	void Server_SetDesiredGait(EALSGait NewGait);

	UFUNCTION(BlueprintGetter, Category = "ALS|Input")
	EALSRotationMode GetDesiredRotationMode() const { return DesiredRotationMode; }

	UFUNCTION(BlueprintSetter, Category = "ALS|Input")
	void SetDesiredRotationMode(EALSRotationMode NewRotMode);

	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "ALS|Character States")
	void Server_SetDesiredRotationMode(EALSRotationMode NewRotMode);

	// Utility function to determine the indended movement direction.
	UFUNCTION(BlueprintCallable, Category = "ALS|Input")
	virtual FVector GetMovementDirection() const { return FVector::ZeroVector; }

	/** Rotation System */

	UFUNCTION(BlueprintCallable, Category = "ALS|Rotation System")
	void SetActorLocationAndTargetRotation(FVector NewLocation, FRotator NewRotation);

	/** Mantle System */

	/** Implement on BP to get correct mantle parameter set according to character state */
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "ALS|Mantle System")
	FALSMantleAsset GetMantleAsset(EALSMantleType MantleType);

	UFUNCTION(BlueprintCallable, Category = "ALS|Mantle System")
	virtual bool MantleCheckGrounded();

	UFUNCTION(BlueprintCallable, Category = "ALS|Mantle System")
	virtual bool MantleCheckFalling();

	/** Automatically called mantle check for vaulting small objects. */
	UFUNCTION(BlueprintCallable, Category = "ALS|Mantle System")
	virtual bool MantleCheckVault();

	/** Movement System */

	UFUNCTION(BlueprintGetter, Category = "ALS|Movement System")
	bool HasMovementInput() const { return bHasMovementInput; }

	UFUNCTION(BlueprintCallable, Category = "ALS|Movement System")
	void SetHasMovementInput(bool bNewHasMovementInput);

	UFUNCTION(BlueprintCallable, Category = "ALS|Movement System")
	FALSMovementSettings GetTargetMovementSettings() const;

	UFUNCTION(BlueprintCallable, Category = "ALS|Movement System")
	EALSGait GetAllowedGait() const;

	UFUNCTION(BlueprintCallable, Category = "ALS|Movement States")
	EALSGait GetActualGait(EALSGait AllowedGait) const;

	UFUNCTION(BlueprintCallable, Category = "ALS|Movement System")
	bool CanSprint() const;

	/** BP implementable function that called when Breakfall starts */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "ALS|Movement System")
	void OnBreakfall();
	virtual void OnBreakfall_Implementation();

	/** BP implementable function that called when A Montage starts, e.g. during rolling */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "ALS|Movement System")
	void Replicated_PlayMontage(UAnimMontage* Montage, float Track);
	virtual void Replicated_PlayMontage_Implementation(UAnimMontage* Montage, float Track);

	/** Implement on BP to get required roll animation according to character's state */
	UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, Category = "ALS|Movement System")
	UAnimMontage* GetRollAnimation();

	/** Utility */

	UFUNCTION(BlueprintCallable, Category = "ALS|Utility")
	float GetAnimCurveValue(FName CurveName) const;

	// 
	UFUNCTION(BlueprintPure, Category = "Utilities")
	float GetAbsoluteAltitude() const;

	// Compares absolute altitude against the altitude falloff, resulting in a float representing atmosphere pressure at
	// the current altitude.
	UFUNCTION(BlueprintPure, Category = "Utilities")
	float GetAtmospherePressure() const;

	/** Implement on BP to draw debug spheres */
	UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, Category = "ALS|Debug")
	void DrawDebugSpheres();

	/** Essential Information Getters/Setters */

	UFUNCTION(BlueprintGetter, Category = "ALS|Essential Information")
	FVector GetAcceleration() const { return Acceleration; }

	// Gets the input direction in local space.
	UFUNCTION(BlueprintGetter, Category = "ALS|Essential Information")
	FVector GetInputAcceleration() const;

	UFUNCTION(BlueprintCallable, Category = "ALS|Essential Information")
	void SetAcceleration(const FVector& NewAcceleration);

	UFUNCTION(BlueprintGetter, Category = "ALS|Essential Information")
	bool IsMoving() const { return bIsMoving; }

	UFUNCTION(BlueprintCallable, Category = "ALS|Essential Information")
	void SetIsMoving(bool bNewIsMoving);

	UFUNCTION(BlueprintCallable, Category = "ALS|Essential Information")
	FVector GetMovementInput() const;

	UFUNCTION(BlueprintGetter, Category = "ALS|Essential Information")
	float GetMovementInputAmount() const { return MovementInputAmount; }

	UFUNCTION(BlueprintCallable, Category = "ALS|Essential Information")
	void SetMovementInputAmount(float NewMovementInputAmount);

	UFUNCTION(BlueprintGetter, Category = "ALS|Essential Information")
	float GetSpeed() const { return Speed; }

	UFUNCTION(BlueprintCallable, Category = "ALS|Essential Information")
	void SetSpeed(float NewSpeed);

	UFUNCTION(BlueprintCallable, Category = "ALS|Essential Information")
	FRotator GetAimingRotation() const { return AimingRotation; }

	UFUNCTION(BlueprintGetter, Category = "ALS|Essential Information")
	float GetAimYawRate() const { return AimYawRate; }

	UFUNCTION(BlueprintCallable, Category = "ALS|Essential Information")
	void SetAimYawRate(float NewAimYawRate);

protected:
	/** Ragdoll System */

	void RagdollUpdate(float DeltaTime);
	void SetActorLocationDuringRagdoll(float DeltaTime);

	/** State Changes */
	virtual void OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode = 0) override;
	virtual void OnMovementStateChanged(EALSMovementState PreviousState);
	virtual void OnMovementActionChanged(EALSMovementAction PreviousAction);
	virtual void OnStanceChanged(EALSStance PreviousStance);
	virtual void OnRotationModeChanged(EALSRotationMode PreviousRotationMode);
	virtual void OnFlightModeChanged(EALSFlightMode PreviousFlightMode);
	virtual void OnGaitChanged(EALSGait PreviousGait);
	virtual void OnOverlayStateChanged(EALSOverlayState PreviousState);

	// Crouching overrides
	virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;

	// Jumping overrides	
	virtual void OnJumped_Implementation() override;
	virtual void Landed(const FHitResult& Hit) override;

	void OnLandFrictionReset() const;

	void SetEssentialValues(float DeltaTime);

	void UpdateCharacterMovement(float DeltaTime);

	// Adjusts walking speed to account for player temperature and ground incline, where extremes of each slow movement.
	float AdjustNewWalkingSpeed(float DeltaTime, float NewSpeed) const;

	// Adjusts walking speed to account for player temperature.
	float AdjustNewFlyingSpeed(float DeltaTime, float NewSpeed) const;

	// Adjusts walking speed to account for player temperature.
	float AdjustNewSwimmingSpeed(float DeltaTime, float NewSpeed) const;

	void UpdateFlightMovement(float DeltaTime);

	// This is the not replicated version that uses the curve data, since desync isn't an issue when standalone.
	void UpdateDynamicMovementSettingsStandalone(float DeltaTime, EALSGait AllowedGait);

	// This is the shorter replicated version that doesn't use the additional curve data.
	void UpdateDynamicMovementSettingsNetworked(float DeltaTime, EALSGait AllowedGait);

	// A complete update that is replicated and uses curves. Only use if you want to risk the desync. Enabled with bForceFullNetworkedDynamicMovement
	void UpdateDynamicMovementSettingsFull(float DeltaTime, EALSGait AllowedGait);

	void UpdateGroundedRotation(float DeltaTime);
	void UpdateFallingRotation(float DeltaTime);
	void UpdateFlightRotation(float DeltaTime);
	void UpdateSwimmingRotation(float DeltaTime);

	/** Flight System */

	/** This must be overriden to setup custom conditions for allowing character flight. By default this checks for
	 * weight, and temperature limits, and calls the BP implementable version of this, as well. */
	UFUNCTION(BlueprintCallable, Category = "ALS|Flight")
	virtual bool CanFly();

	/** This must be overriden to setup custom conditions for allowing character flight from blueprint. */
	UFUNCTION(BlueprintImplementableEvent, Category = "ALS|Flight")
	bool FlightCheck();

	bool FlightInterruptThresholdCheck() const;

	UFUNCTION(BlueprintImplementableEvent, Category = "ALS|Flight")
	bool FlightInterruptCustomCheck(UPrimitiveComponent* MyComp, AActor* Other, UPrimitiveComponent* OtherComp,
									bool bSelfMoved, FVector HitLocation, FVector HitNormal, FVector NormalImpulse,
									const FHitResult& Hit);

	/** Mantle System */

	virtual void MantleStart(float MantleHeight, const FALSComponentAndTransform& MantleLedgeWS,
							 EALSMantleType MantleType);

	virtual bool MantleCheck(const FALSMantleTraceSettings& TraceSettings);

	/** Place for designers to implement gameplay specific checks for allowing the player to mantle */
	UFUNCTION(BlueprintImplementableEvent, Category = "ALS|Mantle System")
	bool CanMantle(EALSMantleType Type);

	UFUNCTION()
	virtual void MantleUpdate(float BlendIn);

	UFUNCTION()
	virtual void MantleEnd();

	/** Utils */

	float GetMappedSpeed() const;

	void SmoothCharacterRotation(FRotator Target, float TargetInterpSpeed, float ActorInterpSpeed, float DeltaTime);

	float CalculateGroundedRotationRate() const;
	float CalculateFlightRotationRate() const;

	void UpdateRelativeAltitude();

	// Gets the relative altitude of the player, measuring down to a point below the character.
	UFUNCTION(BlueprintCallable, Category = "ALS|Flight")
	float FlightDistanceCheck(float CheckDistance, FVector Direction) const;

	void LimitRotation(float AimYawMin, float AimYawMax, float InterpSpeed, float DeltaTime);

	void SetMovementModel();

	/** Replication */
	UFUNCTION()
	void OnRep_RotationMode(EALSRotationMode PrevRotMode);

	UFUNCTION()
	void OnRep_FlightMode(EALSFlightMode PrevFlightMode);

	UFUNCTION()
	void OnRep_OverlayState(EALSOverlayState PrevOverlayState);

protected:
	/* Custom movement component*/
	UPROPERTY()
	UALSCharacterMovementComponent* MyCharacterMovementComponent;

	/* Optimization */

	/*
	 By default, use of the movement curves is skipped when not standalone to avoid net corrections and desync
	 This overrides that and forces it on. Useful for testing.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS|Optimization")
	bool bForceFullNetworkedDynamicMovement = false;

	/** Input */

	UPROPERTY(EditAnywhere, Replicated, BlueprintReadWrite, Category = "ALS|Input")
	EALSRotationMode DesiredRotationMode = EALSRotationMode::LookingDirection;

	UPROPERTY(EditAnywhere, Replicated, BlueprintReadWrite, Category = "ALS|Input")
	EALSGait DesiredGait = EALSGait::GaitNormal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "ALS|Input")
	EALSStance DesiredStance = EALSStance::Standing;

	/** Movement System */

	UPROPERTY(BlueprintReadOnly, Category = "ALS|Movement System")
	FALSMovementSettings CurrentMovementSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ALS|Movement System")
	FDataTableRowHandle MovementModel;

	UPROPERTY(BlueprintReadOnly, Category = "ALS|Movement System")
	FALSMovementStateSettings MovementData;

	// How much walking speed is affected by the incline of the floor.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ALS|Movement System")
	float WalkingSpeedInclineBias = 2;

	// The speed that the character adjusts to changes in walking incline.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ALS|Movement System")
	float WalkingSpeedInterpRate = 4;

	/** Components */

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ALS|Components")
	UTimelineComponent* MantleTimeline = nullptr;

	/** Essential Information */

	UPROPERTY(BlueprintReadOnly, Category = "ALS|Essential Information")
	FVector Acceleration = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "ALS|Essential Information")
	bool bIsMoving = false;

	UPROPERTY(BlueprintReadOnly, Category = "ALS|Essential Information")
	bool bHasMovementInput = false;

	UPROPERTY(BlueprintReadOnly, Category = "ALS|Essential Information")
	FRotator LastVelocityRotation;

	UPROPERTY(BlueprintReadOnly, Category = "ALS|Essential Information")
	FRotator LastMovementInputRotation;

	UPROPERTY(BlueprintReadOnly, Category = "ALS|Essential Information")
	float Speed = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "ALS|Essential Information")
	float MovementInputAmount = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "ALS|Essential Information")
	float AimYawRate = 0.0f;

	/** Replicated Essential Information*/

	UPROPERTY(BlueprintReadOnly, Category = "ALS|Essential Information")
	float EasedMaxAcceleration = 0.0f;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "ALS|Essential Information")
	FVector ReplicatedCurrentAcceleration = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "ALS|Essential Information")
	FRotator ReplicatedControlRotation = FRotator::ZeroRotator;

	/** State Values */

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ALS|State Values", ReplicatedUsing = OnRep_OverlayState)
	EALSOverlayState OverlayState = EALSOverlayState::Default;

	UPROPERTY(BlueprintReadOnly, Category = "ALS|State Values")
	EALSMovementState MovementState = EALSMovementState::None;

	UPROPERTY(BlueprintReadOnly, Category = "ALS|State Values")
	EALSMovementState PrevMovementState = EALSMovementState::None;

	UPROPERTY(BlueprintReadOnly, Category = "ALS|State Values")
	EALSMovementAction MovementAction = EALSMovementAction::None;

	UPROPERTY(BlueprintReadOnly, Category = "ALS|State Values", ReplicatedUsing = OnRep_RotationMode)
	EALSRotationMode RotationMode = EALSRotationMode::LookingDirection;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ALS|State Values", ReplicatedUsing = OnRep_FlightMode)
	EALSFlightMode FlightMode = EALSFlightMode::None;

	UPROPERTY(BlueprintReadOnly, Category = "ALS|State Values")
	EALSGait Gait = EALSGait::GaitSlow;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ALS|State Values")
	EALSStance Stance = EALSStance::Standing;

	/** Rotation System */

	UPROPERTY(BlueprintReadOnly, Category = "ALS|Rotation System")
	FRotator TargetRotation = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly, Category = "ALS|Rotation System")
	FRotator InAirRotation = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly, Category = "ALS|Rotation System")
	float YawOffset = 0.0f;

	/** Flight System */

	// Flag to disable flight.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS|Flight")
	bool bFlightEnabled = false;

	// Flag to call CanFly() per tick. This will disable flight in midair on a false return. When disabled, CanFly() will
	// only be called on takeoff.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS|Flight")
	bool AlwaysCheckFlightConditions = false;

	// Maximum rotation in Yaw, Pitch and Roll, that may be achieved in flight, when traveling at max speed.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ALS|Flight")
	FVector MaxFlightLean = {40, 40, 0};

	// Maximum rotation rate when traveling at RotationVelocityMax.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ALS|Flight")
	float MaxFlightRotationRate = 3.f;

	// The max degree that flight input will consider forward.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ALS|Flight", Meta = (UIMin = 0, UIMax = 90))
	float MaxFlightForwardAngle = 85;

	// Control for the strength of automatic flight input. Used for calculating wing pressure.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS|Flight")
	float FlightStrengthPassive = 6000;

	// Control for the strength of manual flight input. @TODO What is this for?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS|Flight")
	float FlightStrengthActive = 1;

	// @TODO merge these into one linear color curve so data is packed???
	// @TODO if flight lift is reworked, this may be replaced by lift algorithm that generates this. In that case, this
	// should be repurposed to represent atmospheric density, for use in that algorithm.
	// Flight input strength falloff by altitude. Represents the long distance pressure gradient from the thinning of
	// the atmosphere at higher altitudes. Multiplied by TroposphereHeight, so this should be a normalized curve.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ALS|Flight")
	UCurveFloat* AtmosphericPressureFalloff = nullptr;

	// Flight input strength falloff by pressure. Represents the short distance pressure gradient from the downward
	// force of flight against the ground below.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ALS|Flight")
	UCurveFloat* GroundPressureFalloff = nullptr;

	// Condition to trigger flight automatically cutting out. For Custom to work, implement function FlightInterruptCustomCheck();
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ALS|Flight")
	EALSFlightCancelCondition FlightCancelCondition = EALSFlightCancelCondition::VelocityThreshold;

	// The velocity of the hit required to trigger a positive FlightInterruptThresholdCheck.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS|Flight", meta = (EditCondition =
		"FlightCancelCondition == EALSFlightCancelCondition::VelocityThreshold || FlightCancelCondition == EALSFlightCancelCondition::CustomOrThreshold || FlightCancelCondition == EALSFlightCancelCondition::CustomAndThreshold"
	))
	float FlightInterruptThreshold = 600;

	/** Mantle System */

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ALS|Mantle System")
	FALSMantleTraceSettings GroundedTraceSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ALS|Mantle System")
	FALSMantleTraceSettings AutomaticTraceSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ALS|Mantle System")
	FALSMantleTraceSettings FallingTraceSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ALS|Mantle System")
	UCurveFloat* MantleTimelineCurve = nullptr;

	/** If a dynamic object has a velocity bigger than this value, do not start mantle */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ALS|Mantle System")
	float AcceptableVelocityWhileMantling = 10.0f;

	UPROPERTY(BlueprintReadOnly, Category = "ALS|Mantle System")
	FALSMantleParams MantleParams;

	UPROPERTY(BlueprintReadOnly, Category = "ALS|Mantle System")
	FALSComponentAndTransform MantleLedgeLS;

	UPROPERTY(BlueprintReadOnly, Category = "ALS|Mantle System")
	FTransform MantleTarget = FTransform::Identity;

	UPROPERTY(BlueprintReadOnly, Category = "ALS|Mantle System")
	FTransform MantleActualStartOffset = FTransform::Identity;

	UPROPERTY(BlueprintReadOnly, Category = "ALS|Mantle System")
	FTransform MantleAnimatedStartOffset = FTransform::Identity;

	/**  Enables automatically vaulting over short obstacles, when moving toward them. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ALS|Mantle System")
	bool bUseAutoVault = true;

	/** Should the mantle system perform constant checks while falling? */
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "ALS|Mantle System")
	bool bAlwaysCatchIfFalling = true;

	/** Breakfall System */

	/** If player hits to the ground with a specified amount of velocity, switch to breakfall state */
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "ALS|Breakfall System")
	bool bBreakfallOnLand = true;

	/** Flag to tell the breakfall system to activate the next time the character lands. This will set to false immediatlely after. */
	UPROPERTY(BlueprintReadWrite, Category = "ALS|Breakfall System")
	bool bBreakFallNextLanding = false;

	/** If player hits to the ground with an amount of velocity greater than specified value, switch to breakfall state */
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "ALS|Breakfall System", meta = (EditCondition =
		"bBreakfallOnLand"))
	float BreakfallOnLandVelocity = 600.0f;

	/** Ragdoll System */

	/** If player hits to the ground with a specified amount of velocity, switch to ragdoll state */
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "ALS|Ragdoll System")
	bool bRagdollOnLand = false;

	/** If player hits to the ground with an amount of velocity greater than specified value, switch to ragdoll state */
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "ALS|Ragdoll System", meta = (EditCondition = "bRagdollOnLand"))
	float RagdollOnLandVelocity = 1000.0f;

	/** If player starts to freefall while rolling, switch to ragdoll state */
	UPROPERTY(BlueprintReadWrite, Category = "ALS|Ragdoll System")
	bool bRagdollOnRollfall = false;

	UPROPERTY(BlueprintReadOnly, Category = "ALS|Ragdoll System")
	bool bRagdollOnGround = false;

	UPROPERTY(BlueprintReadOnly, Category = "ALS|Ragdoll System")
	bool bRagdollFaceUp = false;

	UPROPERTY(BlueprintReadOnly, Category = "ALS|Ragdoll System")
	FVector LastRagdollVelocity = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "ALS|Ragdoll System")
	FVector TargetRagdollLocation = FVector::ZeroVector;

	/* Server ragdoll pull force storage*/
	float ServerRagdollPull = 0.0f;

	/* Dedicated server mesh default visibility based anim tick option*/
	EVisibilityBasedAnimTickOption DefVisBasedTickOp;

	/** Cached Variables */

	FVector PreviousVelocity = FVector::ZeroVector;

	float PreviousAimYaw = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Cached Variables")
	UALSCharacterAnimInstance* MainAnimInstance = nullptr;

	/* Timer to manage reset of braking friction factor after on landed event */
	FTimerHandle OnLandedFrictionResetTimer;

	/* Smooth out aiming by interping control rotation*/
	FRotator AimingRotation = FRotator::ZeroRotator;

	bool bIsNetworked = false;

	/** AHHH, I hate this, but I wanted to move View Mode to the player only file, and this is the *one* workaround I had to make. */
	bool RestrictAiming = false;

	/////////////////////////////
	// ** WORLD INTERACTION ** //
	// //////////////////////////

	// This is where various optional features I've added are implemented.

public:
	// Utility to implement temperature system. Call this to update character temperature.
	UFUNCTION(BlueprintCallable, Category = "ALS|World Interaction")
	void SetTemperature(float NewTemperature);

	// Utility to implement weight system. Call this to update character weight.
	UFUNCTION(BlueprintCallable, Category = "ALS|World Interaction")
	void SetWeight(float NewWeight);

protected:

	/**
	* All movement speeds are multiplied against this curve when set. Represents how much temperature affects movement.
	* X = Grounded curve.
	* Y = Flying curve.
	* Z = Swimming curve.
	*/
	UPROPERTY(EditDefaultsOnly, Category = "ALS|World Interaction")
	UCurveVector* TemperatureAffectCurve = nullptr;

	/**
	* All movement speeds are multiplied against this curve when set. Represents how much weight affects movement.
	* X = Grounded curve.
	* Y = Flying curve.
	* Z = Swimming curve.
	*/
	UPROPERTY(EditDefaultsOnly, Category = "ALS|World Interaction")
	UCurveVector* WeightAffectCurve = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "ALS|World Interaction")
	float WeightAffectScale = 100;

private:

	// Altitude variables for flight calculations.
	float SeaAltitude, TroposphereHeight, RelativeAltitude = 0;

	// The current temperature of the player. Cached here, but should rely on another system for proper implementation.
	float Temperature = 0;

	// Cached time from TemperatureAffectCurve at Temperature.
	FVector TemperatureAffect = FVector::OneVector;

	// The temperature that the character must be between for flight to be allowed.
	FVector2D FlightTempBounds = {0, 40};

	// Control for the strength of manual flight input.
	float EffectiveWeight = 30;

	// Cached time from WeightAffectCurve at EffectiveWeight.
	FVector WeightAffect = FVector::OneVector;

	float FlightWeightCutOff = 60;
};
