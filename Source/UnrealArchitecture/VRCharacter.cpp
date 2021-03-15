// Fill out your copyright notice in the Description page of Project Settings.


#include "VRCharacter.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Math/Color.h"
#include "DrawDebugHelpers.h"
#include "NavigationSystem.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Curves/CurveFloat.h"
#include "GameFramework/Pawn.h"
#include "Math/Vector.h"
#include "MotionControllerComponent.h"
#include "Kismet/GameplayStatics.h"
#include "HandController.h"

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

	TeleportPath = CreateDefaultSubobject<USplineComponent>(TEXT("TeleportPath"));
	TeleportPath->SetupAttachment(VRRoot);

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

	LeftController = GetWorld()->SpawnActor<AHandController>(HandControllerClass);
	if (LeftController != nullptr)
	{
		LeftController->AttachToComponent(VRRoot, FAttachmentTransformRules::KeepRelativeTransform);
		LeftController->SetOwner(this);
		LeftController->SetHand(EControllerHand::Left);
	}

	RightController = GetWorld()->SpawnActor<AHandController>(HandControllerClass);
	if (RightController != nullptr)
	{
		RightController->AttachToComponent(VRRoot, FAttachmentTransformRules::KeepRelativeTransform);
		RightController->SetOwner(this);
		RightController->SetHand(EControllerHand::Right);
	}

	LeftController->PairController(RightController);
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

bool AVRCharacter::FindTeleportDestination(TArray<FVector>& OutPath, FVector& OutLocation)
{
	FVector Start = RightController->GetActorLocation();
	FVector Look = RightController->GetActorForwardVector();


	FPredictProjectilePathParams Params = FPredictProjectilePathParams(
		TeleportProjectileRadius,
		Start,
		Look * TeleportProjectileSpeed,
		TeleportSimulationTime,
		ECC_Visibility,
		this
	);
	Params.bTraceComplex = true;
	FPredictProjectilePathResult Result;
	bool bHit = UGameplayStatics::PredictProjectilePath(RightController, Params, Result);

	if (!bHit) return false;

	for (FPredictProjectilePathPointData PointData : Result.PathData)
	{
		OutPath.Add(PointData.Location);
	}

	FNavLocation NavLocation;
	bool NavHit = UNavigationSystemV1::GetCurrent(GetWorld())->ProjectPointToNavigation(Result.HitResult.Location, NavLocation, TeleportProjectionExtent);

	if (!NavHit) return false;

	OutLocation = NavLocation.Location;

	return true;
}

void AVRCharacter::UpdateDestinationMarker()
{
	TArray<FVector> Path;
	FVector OutLocation;
	bool NavHit = FindTeleportDestination(Path, OutLocation);
	if (NavHit)
	{
		DrawTeleportPath(Path);
		DestinationMarker->SetWorldLocation(OutLocation);
	}
	else
	{
		TArray<FVector> EmptyPath;
		DrawTeleportPath(EmptyPath);
	}

	DestinationMarker->SetVisibility(NavHit);

}

void AVRCharacter::DrawTeleportPath(const TArray<FVector>& Path)
{
	UpdateSpline(Path);

	for (USplineMeshComponent* SplineMesh : TeleportPathMeshPool)
	{
		SplineMesh->SetVisibility(false);
	}

	int32 Segment = Path.Num() - 1;
	for (int32 i = 0; i < Segment; i++)
	{
		USplineMeshComponent* SplineMesh;
		if (TeleportPathMeshPool.Num() <= i)
		{
			SplineMesh = NewObject<USplineMeshComponent>(this);
			SplineMesh->SetMobility(EComponentMobility::Movable);
			SplineMesh->AttachToComponent(TeleportPath, FAttachmentTransformRules::KeepRelativeTransform);
			SplineMesh->SetStaticMesh(TeleportArchMesh);
			SplineMesh->SetMaterial(0, TeleportArchMaterial);
			SplineMesh->RegisterComponent();

			TeleportPathMeshPool.Add(SplineMesh);
		}

		SplineMesh = TeleportPathMeshPool[i];
		SplineMesh->SetVisibility(true);

		FVector LocalLocationStart, LocalTangentStart, LocalLocationEnd, LocalTangentEnd;
		TeleportPath->GetLocalLocationAndTangentAtSplinePoint(i, LocalLocationStart, LocalTangentStart);
		TeleportPath->GetLocalLocationAndTangentAtSplinePoint(i + 1, LocalLocationEnd, LocalTangentEnd);
		SplineMesh->SetStartAndEnd(LocalLocationStart, LocalTangentStart, LocalLocationEnd, LocalTangentEnd);


	}





}

void AVRCharacter::UpdateSpline(const TArray<FVector>& Path)
{
	TeleportPath->ClearSplinePoints(false);
	for (int32 i = 0; i < Path.Num(); i++)
	{
		FVector LocalPosition = TeleportPath->GetComponentTransform().InverseTransformPosition(Path[i]);
		FSplinePoint Point(i, LocalPosition, ESplinePointType::Curve);
		TeleportPath->AddPoint(Point, false);
	}



	TeleportPath->UpdateSpline();


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
	PlayerInputComponent->BindAction(TEXT("GripLeft"), IE_Pressed, this, &AVRCharacter::GripLeft);
	PlayerInputComponent->BindAction(TEXT("GripLeft"), IE_Released, this, &AVRCharacter::ReleaseLeft);
	PlayerInputComponent->BindAction(TEXT("GripRight"), IE_Pressed, this, &AVRCharacter::GripRight);
	PlayerInputComponent->BindAction(TEXT("GripRight"), IE_Released, this, &AVRCharacter::ReleaseRight);




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
	SetActorLocation(DestinationMarker->GetComponentLocation() + GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * GetActorUpVector());
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

