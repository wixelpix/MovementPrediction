// Fill out your copyright notice in the Description page of Project Settings.


#include "ReallyCoolMovementComponent.h"
#include "MovementPredictionCharacter.h"

void FSavedMove_ReallyCoolMovez::Clear()
{
	Super::Clear();

	bSavedWantsToDash = false;
	SavedDashDir = FVector::ZeroVector;
}

uint8 FSavedMove_ReallyCoolMovez::GetCompressedFlags() const
{
	uint8 Result = Super::GetCompressedFlags();

	// Compress our custom flag into the result if set
	if (bSavedWantsToDash)
	{
		Result |= FSavedMove_Character::FLAG_Custom_0;
	}

	return Result;
}

bool FSavedMove_ReallyCoolMovez::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* Character, float MaxDelta) const
{
	// Engine tries to combine moves for optimization purposes. If we differ from the new move, we probably shouldn't combine them.
	if (bSavedWantsToDash != ((FSavedMove_ReallyCoolMovez*)&NewMove)->bSavedWantsToDash)
	{
		return false;
	}

	if (SavedDashDir != ((FSavedMove_ReallyCoolMovez*)&NewMove)->SavedDashDir)
	{
		return false;
	}

	return Super::CanCombineWith(NewMove, Character, MaxDelta);
}

void FSavedMove_ReallyCoolMovez::SetMoveFor(ACharacter* Character, float InDeltaTime, const FVector& NewAccel, FNetworkPredictionData_Client_Character& ClientData)
{
	Super::SetMoveFor(Character, InDeltaTime, NewAccel, ClientData);

	check(Character);

	UReallyCoolMovementComponent* Movement = Cast<UReallyCoolMovementComponent>(Character->GetCharacterMovement());
	if (Movement)
	{
		// Save the state from the player's input, set on movement component
		bSavedWantsToDash = Movement->bWantsToDash;
		SavedDashDir = Movement->DashDir;
	}
}

void FSavedMove_ReallyCoolMovez::PrepMoveFor(ACharacter* Character)
{
	Super::PrepMoveFor(Character);

	// Similar to update from compressed flags, except for non-flag stuff.
	// We shouldn't update flags here.

	// This is only used for predictive stuff. We still have to send relevant data through RPCs
	UReallyCoolMovementComponent* Movement = Cast<UReallyCoolMovementComponent>(Character->GetCharacterMovement());
	if (Movement)
	{
		Movement->DashDir = SavedDashDir;
	}
}

FNetworkPredictionData_Client_ReallyCoolMovez::FNetworkPredictionData_Client_ReallyCoolMovez(const UCharacterMovementComponent& ClientMovement)
	: Super(ClientMovement)
{

}

FSavedMovePtr FNetworkPredictionData_Client_ReallyCoolMovez::AllocateNewMove()
{
	return FSavedMovePtr(new FSavedMove_ReallyCoolMovez());
}

UReallyCoolMovementComponent::UReallyCoolMovementComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DashSpeed = 1000.f;
	DashDurationSeconds = 0.25f;
	DashDir = FVector::ZeroVector;
	DashTimeRemaining = 0.f;
}

void UReallyCoolMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);

	// Resets the movement component to the state when the move was made so we can resimulate
	bWantsToDash = (Flags & FSavedMove_ReallyCoolMovez::FLAG_Custom_0) != 0;
}

FNetworkPredictionData_Client* UReallyCoolMovementComponent::GetPredictionData_Client() const
{
	check(PawnOwner);
	//check(PawnOwner->GetLocalRole() < ROLE_Authority);

	if (!ClientPredictionData)
	{
		UReallyCoolMovementComponent* MutableThis = const_cast<UReallyCoolMovementComponent*>(this);

		MutableThis->ClientPredictionData = new FNetworkPredictionData_Client_ReallyCoolMovez(*this);
		MutableThis->ClientPredictionData->MaxSmoothNetUpdateDist = 92.f;
		MutableThis->ClientPredictionData->NoSmoothNetUpdateDist = 140.f;
	}

	return ClientPredictionData;
}

void UReallyCoolMovementComponent::OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity)
{
	Super::OnMovementUpdated(DeltaSeconds, OldLocation, OldVelocity);

	if (!CharacterOwner)
	{
		return;
	}

	// Set our dash movement
	if (bWantsToDash)
	{
		DashTimeRemaining = DashDurationSeconds;
		bWantsToDash = false;
	}

	// Update dash
	if (DashTimeRemaining > 0.f)
	{
		Launch(DashDir * DashSpeed);
		DashTimeRemaining -= DeltaSeconds;
	}
}

void UReallyCoolMovementComponent::StartDash(const FVector& DashDirection)
{
	DashDir = DashDirection;
	bWantsToDash = true;
}

