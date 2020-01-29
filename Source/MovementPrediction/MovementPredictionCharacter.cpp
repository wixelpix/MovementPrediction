// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "MovementPredictionCharacter.h"
#include "MovementPredictionProjectile.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/InputSettings.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "MotionControllerComponent.h"
#include "XRMotionControllerBase.h" // for FXRMotionControllerBase::RightHandSourceId
#include "ReallyCoolMovementComponent.h"
#include "Net/UnrealNetwork.h"

DEFINE_LOG_CATEGORY_STATIC(LogFPChar, Warning, All);

//////////////////////////////////////////////////////////////////////////
// AMovementPredictionCharacter

AMovementPredictionCharacter::AMovementPredictionCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UReallyCoolMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(55.f, 96.0f);

	// set our turn rates for input
	BaseTurnRate = 45.f;
	BaseLookUpRate = 45.f;

	// Create a CameraComponent	
	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCameraComponent->SetupAttachment(GetCapsuleComponent());
	FirstPersonCameraComponent->SetRelativeLocation(FVector(-39.56f, 1.75f, 64.f)); // Position the camera
	FirstPersonCameraComponent->bUsePawnControlRotation = true;

	// Create a mesh component that will be used when being viewed from a '1st person' view (when controlling this pawn)
	Mesh1P = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CharacterMesh1P"));
	Mesh1P->SetOnlyOwnerSee(true);
	Mesh1P->SetupAttachment(FirstPersonCameraComponent);
	Mesh1P->bCastDynamicShadow = false;
	Mesh1P->CastShadow = false;
	Mesh1P->SetRelativeRotation(FRotator(1.9f, -19.19f, 5.2f));
	Mesh1P->SetRelativeLocation(FVector(-0.5f, -4.4f, -155.7f));

	// Create a gun mesh component
	FP_Gun = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FP_Gun"));
	FP_Gun->SetOnlyOwnerSee(true);			// only the owning player will see this mesh
	FP_Gun->bCastDynamicShadow = false;
	FP_Gun->CastShadow = false;
	// FP_Gun->SetupAttachment(Mesh1P, TEXT("GripPoint"));
	FP_Gun->SetupAttachment(RootComponent);

	FP_MuzzleLocation = CreateDefaultSubobject<USceneComponent>(TEXT("MuzzleLocation"));
	FP_MuzzleLocation->SetupAttachment(FP_Gun);
	FP_MuzzleLocation->SetRelativeLocation(FVector(0.2f, 48.4f, -10.6f));

	// Default offset from the character location for projectiles to spawn
	GunOffset = FVector(100.0f, 0.0f, 10.0f);

	DashDurationSeconds = 0.25f;
	DashSpeed = 1000.f;
	DashTimeLeft = 0.f;

	bReplicates = true;
	bUseMovementPrediction = true;
}

void AMovementPredictionCharacter::BeginPlay()
{
	// Call the base class  
	Super::BeginPlay();

	//Attach gun mesh component to Skeleton, doing it here because the skeleton is not yet created in the constructor
	FP_Gun->AttachToComponent(Mesh1P, FAttachmentTransformRules(EAttachmentRule::SnapToTarget, true), TEXT("GripPoint"));

	// Show or hide the two versions of the gun based on whether or not we're using motion controllers.
	Mesh1P->SetHiddenInGame(false, true);
}

//////////////////////////////////////////////////////////////////////////
// Input

void AMovementPredictionCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	// set up gameplay key bindings
	check(PlayerInputComponent);

	// Bind jump events
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);

	// Bind fire event
	PlayerInputComponent->BindAction("Fire", IE_Pressed, this, &AMovementPredictionCharacter::OnFire);

	// Bind movement events
	PlayerInputComponent->BindAxis("MoveForward", this, &AMovementPredictionCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &AMovementPredictionCharacter::MoveRight);

	// We have 2 versions of the rotation bindings to handle different kinds of devices differently
	// "turn" handles devices that provide an absolute delta, such as a mouse.
	// "turnrate" is for devices that we choose to treat as a rate of change, such as an analog joystick
	PlayerInputComponent->BindAxis("Turn", this, &APawn::AddControllerYawInput);
	PlayerInputComponent->BindAxis("TurnRate", this, &AMovementPredictionCharacter::TurnAtRate);
	PlayerInputComponent->BindAxis("LookUp", this, &APawn::AddControllerPitchInput);
	PlayerInputComponent->BindAxis("LookUpRate", this, &AMovementPredictionCharacter::LookUpAtRate);

	PlayerInputComponent->BindAction("TogglePrediction", IE_Pressed, this, &AMovementPredictionCharacter::ToggleMovementPrediction);

	// Dash input
	PlayerInputComponent->BindAction("Dash", IE_Pressed, this, &ThisClass::OnStartDash);
	PlayerInputComponent->BindAction("Dash", IE_Released, this, &ThisClass::OnStopDash);
}

void AMovementPredictionCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AMovementPredictionCharacter, CurrentDashDir);
	DOREPLIFETIME(AMovementPredictionCharacter, DashTimeLeft);
}

void AMovementPredictionCharacter::OnFire()
{
	// try and fire a projectile
	if (ProjectileClass != NULL)
	{
		UWorld* const World = GetWorld();
		if (World != NULL)
		{
			const FRotator SpawnRotation = GetControlRotation();
			// MuzzleOffset is in camera space, so transform it to world space before offsetting from the character location to find the final muzzle position
			const FVector SpawnLocation = ((FP_MuzzleLocation != nullptr) ? FP_MuzzleLocation->GetComponentLocation() : GetActorLocation()) + SpawnRotation.RotateVector(GunOffset);

			//Set Spawn Collision Handling Override
			FActorSpawnParameters ActorSpawnParams;
			ActorSpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;

			// spawn the projectile at the muzzle
			World->SpawnActor<AMovementPredictionProjectile>(ProjectileClass, SpawnLocation, SpawnRotation, ActorSpawnParams);
		}
	}

	// try and play the sound if specified
	if (FireSound != NULL)
	{
		UGameplayStatics::PlaySoundAtLocation(this, FireSound, GetActorLocation());
	}

	// try and play a firing animation if specified
	if (FireAnimation != NULL)
	{
		// Get the animation object for the arms mesh
		UAnimInstance* AnimInstance = Mesh1P->GetAnimInstance();
		if (AnimInstance != NULL)
		{
			AnimInstance->Montage_Play(FireAnimation, 1.f);
		}
	}
}

void AMovementPredictionCharacter::MoveForward(float Value)
{
	if (Value != 0.0f)
	{
		// add movement in that direction
		AddMovementInput(GetActorForwardVector(), Value);
	}
}

void AMovementPredictionCharacter::MoveRight(float Value)
{
	if (Value != 0.0f)
	{
		// add movement in that direction
		AddMovementInput(GetActorRightVector(), Value);
	}
}

void AMovementPredictionCharacter::TurnAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerYawInput(Rate * BaseTurnRate * GetWorld()->GetDeltaSeconds());
}

void AMovementPredictionCharacter::LookUpAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerPitchInput(Rate * BaseLookUpRate * GetWorld()->GetDeltaSeconds());
}

void AMovementPredictionCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Update dash
	if (DashTimeLeft > 0.f)
	{
		LaunchCharacter(CurrentDashDir * DashSpeed, true, true);
		DashTimeLeft -= DeltaSeconds;
	}
}

void AMovementPredictionCharacter::ToggleMovementPrediction()
{
	ServerRPC_ToggleMovementPrediction();
}

void AMovementPredictionCharacter::MulticastRPC_ToggleMovementPrediction_Implementation()
{
	bUseMovementPrediction = !bUseMovementPrediction;

	if (bUseMovementPrediction)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Prediction turned ON!"));
	}
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Prediction turned OFF!"));
	}
}

void AMovementPredictionCharacter::ServerRPC_ToggleMovementPrediction_Implementation()
{
	MulticastRPC_ToggleMovementPrediction();
}

bool AMovementPredictionCharacter::ServerRPC_ToggleMovementPrediction_Validate()
{
	return true;
}

void AMovementPredictionCharacter::OnStartDash()
{
	FVector Direction = GetControlRotation().Vector().GetSafeNormal2D();
	StartDash(Direction);
}

void AMovementPredictionCharacter::OnStopDash()
{
	// todo
}

void AMovementPredictionCharacter::StartDash(const FVector& DashDir, bool bFromReplication /*= false*/)
{
	if (IsLocallyControlled())
	{
		if (bFromReplication)
		{
			return;
		}

		ServerRPC_StartDash(DashDir);
	}

	// Start the dash on the movement component
	if (bUseMovementPrediction)
	{
		if (UReallyCoolMovementComponent* Movement = Cast<UReallyCoolMovementComponent>(GetCharacterMovement()))
		{
			Movement->StartDash(DashDir);
		}
	}
	else // Do our jank version without prediction
	{
		CurrentDashDir = DashDir;
		DashTimeLeft = DashDurationSeconds;
	}
}

void AMovementPredictionCharacter::StopDash(bool bFromReplication /*= false*/)
{
	// todo
}

void AMovementPredictionCharacter::MulticastRPC_StopDash_Implementation()
{
	StopDash(true);
}

void AMovementPredictionCharacter::ServerRPC_StopDash_Implementation()
{
	MulticastRPC_StopDash();
}

bool AMovementPredictionCharacter::ServerRPC_StopDash_Validate()
{
	return true;
}

void AMovementPredictionCharacter::MulticastRPC_StartDash_Implementation(const FVector& DashDir)
{
	StartDash(DashDir, true);
}

void AMovementPredictionCharacter::ServerRPC_StartDash_Implementation(const FVector& DashDir)
{
	MulticastRPC_StartDash(DashDir);
}

bool AMovementPredictionCharacter::ServerRPC_StartDash_Validate(const FVector& DashDir)
{
	return true;
}
