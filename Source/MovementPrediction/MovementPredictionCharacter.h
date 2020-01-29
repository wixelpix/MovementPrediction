// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "MovementPredictionCharacter.generated.h"

class UInputComponent;

UCLASS(config=Game)
class AMovementPredictionCharacter : public ACharacter
{
	GENERATED_UCLASS_BODY()

	/** Pawn mesh: 1st person view (arms; seen only by self) */
	UPROPERTY(VisibleDefaultsOnly, Category = Mesh)
	class USkeletalMeshComponent* Mesh1P;

	/** Gun mesh: 1st person view (seen only by self) */
	UPROPERTY(VisibleDefaultsOnly, Category = Mesh)
	class USkeletalMeshComponent* FP_Gun;

	/** Location on gun mesh where projectiles should spawn. */
	UPROPERTY(VisibleDefaultsOnly, Category = Mesh)
	class USceneComponent* FP_MuzzleLocation;

	/** First person camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	class UCameraComponent* FirstPersonCameraComponent;

protected:
	virtual void BeginPlay();

public:
	/** Base turn rate, in deg/sec. Other scaling may affect final turn rate. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera)
	float BaseTurnRate;

	/** Base look up/down rate, in deg/sec. Other scaling may affect final rate. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera)
	float BaseLookUpRate;

	/** Gun muzzle's offset from the characters location */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Gameplay)
	FVector GunOffset;

	/** Projectile class to spawn */
	UPROPERTY(EditDefaultsOnly, Category = Projectile)
	TSubclassOf<class AMovementPredictionProjectile> ProjectileClass;

	/** Sound to play each time we fire */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Gameplay)
	class USoundBase* FireSound;

	/** AnimMontage to play each time we fire */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Gameplay)
	class UAnimMontage* FireAnimation;

protected:

	/** Fires a projectile. */
	void OnFire();

	/** Handles moving forward/backward */
	void MoveForward(float Val);

	/** Handles stafing movement, left and right */
	void MoveRight(float Val);

	/**
	 * Called via input to turn at a given rate.
	 * @param Rate	This is a normalized rate, i.e. 1.0 means 100% of desired turn rate
	 */
	void TurnAtRate(float Rate);

	/**
	 * Called via input to turn look up/down at a given rate.
	 * @param Rate	This is a normalized rate, i.e. 1.0 means 100% of desired turn rate
	 */
	void LookUpAtRate(float Rate);

protected:
	// APawn interface
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	// End of APawn interface

public:
	/** Returns Mesh1P subobject **/
	FORCEINLINE class USkeletalMeshComponent* GetMesh1P() const { return Mesh1P; }
	/** Returns FirstPersonCameraComponent subobject **/
	FORCEINLINE class UCameraComponent* GetFirstPersonCameraComponent() const { return FirstPersonCameraComponent; }

private:

	void ToggleMovementPrediction();

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerRPC_ToggleMovementPrediction();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastRPC_ToggleMovementPrediction();

	bool bUseMovementPrediction;

#pragma region Dash
protected:

	/** Input handler for dash start */
	void OnStartDash();

	/** Input handler for dash stop */
	void OnStopDash();

	void StartDash(const FVector& DashDir, bool bFromReplication = false);
	void StopDash(bool bFromReplication = false);

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerRPC_StartDash(const FVector& DashDir);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastRPC_StartDash(const FVector& DashDir);

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerRPC_StopDash();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastRPC_StopDash();

	UPROPERTY(EditDefaultsOnly, Category = "Dash")
	float DashDurationSeconds;

	UPROPERTY(EditDefaultsOnly, Category = "Dash")
	float DashSpeed;

	UPROPERTY(Transient, Replicated)
	FVector CurrentDashDir;

	UPROPERTY(Transient, Replicated)
	float DashTimeLeft;

#pragma endregion

};

