// Fill out your copyright notice in the Description page of Project Settings.


#include "VRCharacter.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Math/Color.h"
#include "Components/CapsuleComponent.h"

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

	
}

// Called when the game starts or when spawned
void AVRCharacter::BeginPlay()
{


	Super::BeginPlay();
	
	
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

}

void AVRCharacter::UpdateDestinationMarker()
{
	FVector Start = Camera->GetComponentLocation();
	FVector End = Start + Camera->GetForwardVector() * MaxTeleportDistance;

	FHitResult HitResult;
	bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECC_Visibility);
	//DrawDebugLine(this->GetWorld(), Camera->GetComponentLocation(), Camera->GetComponentLocation() + Camera->GetForwardVector() * 9000, FColor(0,0,0),false,-1.0F,(uint8)'\000',5.0f);
	if (bHit) 
	{
		DestinationMarker->SetWorldLocation(HitResult.Location);

	}
	DestinationMarker->SetVisibility(bHit);
	
}

// Called to bind functionality to input
void AVRCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	PlayerInputComponent->BindAxis(TEXT("Forward"), this, &AVRCharacter::MoveForward);
	PlayerInputComponent->BindAxis(TEXT("Right"), this, &AVRCharacter::MoveRight);
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

void AVRCharacter::BeginTeleport()
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (PC != nullptr)
	{
		PC->PlayerCameraManager->StartCameraFade(0, 1, TeleportFadeTime, FLinearColor::Black);
	}

	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle,this, &AVRCharacter::FinishTeleport, TeleportFadeTime, false);


}

void AVRCharacter::FinishTeleport()
{
	
	SetActorLocation(DestinationMarker->GetComponentLocation() + FVector(0,0,GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (PC != nullptr)
	{
		PC->PlayerCameraManager->StartCameraFade(1, 0, TeleportFadeTime, FLinearColor::Black);
	}
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


