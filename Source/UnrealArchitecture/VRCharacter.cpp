// Fill out your copyright notice in the Description page of Project Settings.


#include "VRCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"

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


}

// Called to bind functionality to input
void AVRCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	PlayerInputComponent->BindAxis(TEXT("Move_Y"), this, &AVRCharacter::MoveForward);
	PlayerInputComponent->BindAxis(TEXT("Move_X"), this, &AVRCharacter::MoveRight);
	PlayerInputComponent->BindAxis(TEXT("Rotate_X"), this, &AVRCharacter::Rotate_X);
	PlayerInputComponent->BindAxis(TEXT("Rotate_Y"), this, &AVRCharacter::Rotate_Y);

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


