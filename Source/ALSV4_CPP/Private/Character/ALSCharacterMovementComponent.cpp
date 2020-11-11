// Project:         Advanced Locomotion System V4 on C++
// Source Code:     https://github.com/dyanikoglu/ALSV4_CPP
// Original Author: Haziq Fadhil
// Contributors:    Doga Can Yanikoglu


#include "Character/ALSCharacterMovementComponent.h"
#include "Character/ALSBaseCharacter.h"

UALSCharacterMovementComponent::UALSCharacterMovementComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UALSCharacterMovementComponent::OnMovementUpdated(const float DeltaTime, const FVector& OldLocation, const FVector& OldVelocity)
{
	Super::OnMovementUpdated(DeltaTime, OldLocation, OldVelocity);

	if (!CharacterOwner)
	{
		return;
	}

	// Set Movement Settings
	if (bRequestMovementSettingsChange)
	{
		MaxWalkSpeed = MyNewMaxWalkSpeed;
		MaxWalkSpeedCrouched = MyNewMaxWalkSpeed;
		MaxFlySpeed = MyNewMaxFlySpeed;
		MaxSwimSpeed = MyNewMaxSwimSpeed;
	}
}

void UALSCharacterMovementComponent::UpdateFromCompressedFlags(const uint8 Flags) // Client only
{
	Super::UpdateFromCompressedFlags(Flags);

	bRequestMovementSettingsChange = (Flags & FSavedMove_Character::FLAG_Custom_0) != 0;
}

class FNetworkPredictionData_Client* UALSCharacterMovementComponent::GetPredictionData_Client() const
{
	check(PawnOwner != nullptr);

	if (!ClientPredictionData)
	{
		UALSCharacterMovementComponent* MutableThis = const_cast<UALSCharacterMovementComponent*>(this);

		MutableThis->ClientPredictionData = new FNetworkPredictionData_Client_Faerie(*this);
		MutableThis->ClientPredictionData->MaxSmoothNetUpdateDist = 92.f;
		MutableThis->ClientPredictionData->NoSmoothNetUpdateDist = 140.f;
	}

	return ClientPredictionData;
}

void UALSCharacterMovementComponent::FSavedMove_Faerie::Clear()
{
	Super::Clear();

	bSavedRequestMovementSettingsChange = false;
}

uint8 UALSCharacterMovementComponent::FSavedMove_Faerie::GetCompressedFlags() const
{
	uint8 Result = Super::GetCompressedFlags();

	if (bSavedRequestMovementSettingsChange)
	{
		Result |= FLAG_Custom_0;
	}

	return Result;
}

void UALSCharacterMovementComponent::FSavedMove_Faerie::SetMoveFor(ACharacter* Character,
																   const float InDeltaTime,
																   FVector const& NewAccel,
																   class FNetworkPredictionData_Client_Character& ClientData)
{
	Super::SetMoveFor(Character, InDeltaTime, NewAccel, ClientData);

	UALSCharacterMovementComponent* CharacterMovement = Cast<UALSCharacterMovementComponent>(Character->GetCharacterMovement());
	if (CharacterMovement)
	{
		bSavedRequestMovementSettingsChange = CharacterMovement->bRequestMovementSettingsChange;
	}
}

UALSCharacterMovementComponent::FNetworkPredictionData_Client_Faerie::FNetworkPredictionData_Client_Faerie(
	const UCharacterMovementComponent& ClientMovement)
	: Super(ClientMovement)
{
}

FSavedMovePtr UALSCharacterMovementComponent::FNetworkPredictionData_Client_Faerie::AllocateNewMove()
{
	return MakeShared<FSavedMove_Faerie>();
}

void UALSCharacterMovementComponent::SetMaxWalkingSpeed(const float NewMaxWalkSpeed)
{
	if (PawnOwner->IsLocallyControlled())
	{
		MyNewMaxWalkSpeed = NewMaxWalkSpeed;
		Server_SetMaxWalkingSpeed(NewMaxWalkSpeed);
	}
	bRequestMovementSettingsChange = true;
}

void UALSCharacterMovementComponent::Server_SetMaxWalkingSpeed_Implementation(const float NewMaxWalkSpeed)
{
	MyNewMaxWalkSpeed = NewMaxWalkSpeed;
}

void UALSCharacterMovementComponent::SetMaxFlyingSpeed(const float NewMaxFlySpeed)
{
	if (PawnOwner->IsLocallyControlled())
	{
		MyNewMaxFlySpeed = NewMaxFlySpeed;
		Server_SetMaxFlyingSpeed(NewMaxFlySpeed);
	}
	bRequestMovementSettingsChange = true;
}

void UALSCharacterMovementComponent::Server_SetMaxFlyingSpeed_Implementation(const float NewMaxFlySpeed)
{
	MyNewMaxFlySpeed = NewMaxFlySpeed;
}

void UALSCharacterMovementComponent::SetMaxSwimmingSpeed(const float NewMaxSwimSpeed)
{
	if (PawnOwner->IsLocallyControlled())
	{
		MyNewMaxSwimSpeed = NewMaxSwimSpeed;
		Server_SetMaxSwimmingSpeed(NewMaxSwimSpeed);
	}
	bRequestMovementSettingsChange = true;
}

void UALSCharacterMovementComponent::Server_SetMaxSwimmingSpeed_Implementation(const float NewMaxSwimSpeed)
{
	MyNewMaxSwimSpeed = NewMaxSwimSpeed;
}