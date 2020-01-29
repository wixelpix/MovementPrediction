// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "MovementPredictionGameMode.h"
#include "MovementPredictionHUD.h"
#include "MovementPredictionCharacter.h"
#include "UObject/ConstructorHelpers.h"

AMovementPredictionGameMode::AMovementPredictionGameMode()
	: Super()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnClassFinder(TEXT("/Game/FirstPersonCPP/Blueprints/FirstPersonCharacter"));
	DefaultPawnClass = PlayerPawnClassFinder.Class;

	// use our custom HUD class
	HUDClass = AMovementPredictionHUD::StaticClass();
}
