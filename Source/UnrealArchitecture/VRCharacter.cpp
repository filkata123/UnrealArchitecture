// Fill out your copyright notice in the Description page of Project Settings.


#include "VRCharacter.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Math/Color.h"
#include "Components/CapsuleComponent.h"
#include "Components/PostProcessComponent.h"
#include "NavigationSystem.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Curves/CurveFloat.h"
#include "GameFramework/Pawn.h"
#include "Math/Vector.h"

bool snap = false;
// Sets default values
AVRCharacter::AVRCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	
	VRRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VRRoot"));
	VRRoot->SetupAttachment(GetRootComponent());

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(VRRoot);

	DestinationMarker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DestinationMarker"));
	DestinationMarker->SetupAttachment(GetRootComponent());

	PostProcessComponent = CreateDefaultSubobject<UPostProcessComponent>(TEXT("PostProcessComponent"));
	PostProcessComponent->SetupAttachment(GetRootComponent());

}

// Called when the game starts or when spawned
void AVRCharacter::BeginPlay()
{

	Super::BeginPlay();
	
	if (BlinkerMaterialBase != nullptr)
	{
		BlinkerMaterialInstance = UMaterialInstanceDynamic::Create(BlinkerMaterialBase, this);
		PostProcessComponent->AddOrUpdateBlendable(BlinkerMaterialInstance);
		
	}
	
}

// Called every frame
void AVRCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	/*
	FVector CameraLocation = Camera->GetComponentLocation();
	FVector VRRootLocation = VRRoot->GetComponentLocation();

	SetActorLocation(FVector(CameraLocation.X, CameraLocation.Y, GetActorLocation().Z));
	FVector distanceCameraMoved = GetActorLocation() - CameraLocation;

	VRRoot->SetWorldLocation(FVector((VRRootLocation.X - distanceCameraMoved.X), (VRRootLocation.Y - distanceCameraMoved.Y), VRRootLocation.Z));
	*/

	FVector NewCameraOffset = Camera->GetComponentLocation() - GetActorLocation();
	NewCameraOffset.Z = 0;
	AddActorWorldOffset(NewCameraOffset);
	VRRoot->AddWorldOffset(-NewCameraOffset);

	UpdateDestinationMarker();

	UpdateBlinkers();

}

bool AVRCharacter::FindTeleportDestination(FVector &OutLocation)
{
	FVector Start = Camera->GetComponentLocation();
	FVector End = Start + Camera->GetForwardVector() * MaxTeleportDistance;

	FHitResult HitResult;
	bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECC_Visibility);

	if (!bHit) return false;

	FNavLocation NavLocation;
	bool NavHit = UNavigationSystemV1::GetCurrent(GetWorld())->ProjectPointToNavigation(HitResult.Location, NavLocation, TeleportProjectionExtent);

	if (!NavHit) return false;

	OutLocation = NavLocation.Location;

	return true;
}

void AVRCharacter::UpdateDestinationMarker()
{
	FVector OutLocation;
	bool NavHit = FindTeleportDestination(OutLocation);
	DestinationMarker->SetWorldLocation(OutLocation);	
	DestinationMarker->SetVisibility(NavHit);
	
}

void AVRCharacter::UpdateBlinkers()
{
	if (RadiusVsVelocity == nullptr) return;
	float BlinkerRadius = RadiusVsVelocity->GetFloatValue(GetVelocity().Size());
	BlinkerMaterialInstance->SetScalarParameterValue(TEXT("BlinkerRadius"), BlinkerRadius);

	FVector2D Centre = GetBlinkerCenter();
	BlinkerMaterialInstance->SetVectorParameterValue(TEXT("Centre"),  FLinearColor(Centre.X, Centre.Y, 0));
}

FVector2D AVRCharacter::GetBlinkerCenter()
{
	FVector MovementDirection = GetVelocity().GetSafeNormal();
	if (MovementDirection.IsNearlyZero())
	{
		return FVector2D(0.5, 0.5);
	}
	FVector WorldStationaryLocation;
	if(FVector::DotProduct(Camera->GetForwardVector(), MovementDirection) > 0)
		WorldStationaryLocation = Camera->GetComponentLocation() + MovementDirection * 1000;
	else
		WorldStationaryLocation = Camera->GetComponentLocation() - MovementDirection * 1000;

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (PC == nullptr)
	{
		return FVector2D(0.5, 0.5);
	}

	FVector2D ScreenProjectionLocation;
	PC->ProjectWorldLocationToScreen(WorldStationaryLocation, ScreenProjectionLocation);

	int32 SizeX, SizeY;
	PC->GetViewportSize(SizeX, SizeY);
	ScreenProjectionLocation.X /= SizeX;
	ScreenProjectionLocation.Y /= SizeY;

	return ScreenProjectionLocation;

}

// Called to bind functionality to input
void AVRCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	PlayerInputComponent->BindAxis(TEXT("Move_Y"), this, &AVRCharacter::MoveForward);
	PlayerInputComponent->BindAxis(TEXT("Move_X"), this, &AVRCharacter::MoveRight);
	PlayerInputComponent->BindAxis(TEXT("Rotate_X"), this, &AVRCharacter::Rotate_X);
	PlayerInputComponent->BindAxis(TEXT("Rotate_Y"), this, &AVRCharacter::Rotate_Y);
	PlayerInputComponent->BindAction(TEXT("Teleport"), IE_Released, this, &AVRCharacter::BeginTeleport);

}

void AVRCharacter::MoveForward(float throttle) 
{
	AddMovementInput(Camera->GetForwardVector() * throttle);
}

void AVRCharacter::MoveRight(float throttle) 
{
	AddMovementInput(Camera->GetRightVector() * throttle);

}


void AVRCharacter::Rotate_X(float throttle)
{

	switch (snap)
	{
	case true:
		if (FMath::IsNearlyZero(throttle))
		{
			snap = false;
		}
		break;
	case false:
		if (throttle > 0.49f)
		{
			AddControllerYawInput(15);
			snap = true;
		}
		else if (throttle < -0.49f)
		{
			AddControllerYawInput(-15);
			snap = true;
		}
		break;
	}
	
}

void AVRCharacter::Rotate_Y(float throttle)
{
	//UE_LOG(LogTemp, Warning, TEXT("Y: %f"), throttle);
}

void AVRCharacter::BeginTeleport()
{
	if (DestinationMarker->GetVisibleFlag()) {

		StartFade(0, 1);
		FTimerHandle Handle;
		GetWorldTimerManager().SetTimer(Handle,this, &AVRCharacter::FinishTeleport, TeleportFadeTime, false);

	}
}

void AVRCharacter::FinishTeleport()
{
	SetActorLocation(DestinationMarker->GetComponentLocation() + FVector(0,0,GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
	StartFade(1, 0);
}


void AVRCharacter::StartFade(float FromAlpha, float ToAlpha)
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (PC != nullptr)
	{
		PC->PlayerCameraManager->StartCameraFade(FromAlpha, ToAlpha, TeleportFadeTime + 0.5, FLinearColor::Black);
	}
}

