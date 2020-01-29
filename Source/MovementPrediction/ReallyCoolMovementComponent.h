// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ReallyCoolMovementComponent.generated.h"

class ACharacter;
class FNetworkPredictionData_Client_Character;

class FSavedMove_ReallyCoolMovez : public FSavedMove_Character
{
public:

	typedef FSavedMove_Character Super;

	// Begin FSavedMove_Character Interface

	/** Resets saved variables. */
	virtual void Clear() override;

	/** Stores input commands into compressed flags. */
	virtual uint8 GetCompressedFlags() const override;

	/** Checks whether or not this move can be combined with another */
	virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* Character, float MaxDelta) const override;

	/** Sets the move before sending to the server */
	virtual void SetMoveFor(ACharacter* Character, float InDeltaTime, const FVector& NewAccel, FNetworkPredictionData_Client_Character& ClientData) override;
	
	/** Copies variables from saved move to movement component for prediction correction */
	virtual void PrepMoveFor(ACharacter* Character) override;

	// End FSavedMove_Character Interface

	// Dash input flag - used to re-trigger the ability if a correction forces us to resimulate
	uint8 bSavedWantsToDash : 1;

	// Desired dash direction
	FVector SavedDashDir;
};

class FNetworkPredictionData_Client_ReallyCoolMovez : public FNetworkPredictionData_Client_Character
{
public:
	FNetworkPredictionData_Client_ReallyCoolMovez(const UCharacterMovementComponent& ClientMovement);

	typedef FNetworkPredictionData_Client_Character Super;

	/** Allocates a new copy of the saved move */
	virtual FSavedMovePtr AllocateNewMove() override;
};

/**
 * Really cool movement component with MOVEMENT PREDICTION?!
 */
UCLASS()
class MOVEMENTPREDICTION_API UReallyCoolMovementComponent : public UCharacterMovementComponent
{
	GENERATED_UCLASS_BODY()

	friend class FSavedMove_ReallyCoolMovez;

public:

	// Begin UCharacterMovementComponent Interface
	virtual void UpdateFromCompressedFlags(uint8 Flags) override;
	virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;
	virtual void OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity) override;
	// End UCharacterMovementComponent Interface

	/** Tells the component to start a dash */
	void StartDash(const FVector& DashDirection);

protected:

	// Speed of the dash
	UPROPERTY(EditDefaultsOnly, Category = "Dash")
	float DashSpeed;

	// Duration of the dash
	UPROPERTY(EditDefaultsOnly, Category = "Dash")
	float DashDurationSeconds;

	// Variables we need - movement prediction will touch these
	uint8 bWantsToDash : 1;
	float DashTimeRemaining;
	FVector DashDir;
};
