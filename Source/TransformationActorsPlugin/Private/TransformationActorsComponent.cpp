﻿// Copyright 2020 Anatoli Kucharau. All Rights Reserved.


#include "TransformationActorsComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "TransformationActorsInterface.h"
#include "TimerManager.h"
#include "Camera/CameraComponent.h"

// Sets default values for this component's properties
UTransformationActorsComponent::UTransformationActorsComponent()
{
	TransformState = ETransformState::ETS_Idle;
	bIsTransform = false;

	RollSave = 0.f;
	PitchSave = 0.f;
	YawSave = 0.f;
	DeltaRollDegree = 0.f;
	DeltaPitchDegree = 0.f;
	DeltaYawDegree = 0.f;

	float TimersDeltaTime = 0.017f;
	LocationTimerDeltaTime = TimersDeltaTime;
	RotationTimerDeltaTime = TimersDeltaTime;
	ScaleTimerDeltaTime = TimersDeltaTime;

	LocationSpeed = 25.f;
	bSweep = false;
	LocationDeepSpeed = 25.f;
	ScaleSpeed = 0.015f;
	RotationSpeed = 0.5f;
	SumInputAxisValue = 0.f;

	bIsLockFirstIterationLocationTimer = false;
	bIsLockFirstIterationRotationTimer = false;
	bIsLockFirstIterationScaleTimer = false;

	bIsShowDebugMessages = false;

	LocationSpeedKeyboard = 25.f;
	RotationSpeedKeyboard = 5.f;
	ScaleSpeedKeyboard = 0.1f;

	MinScale = 0.01f;
}

void UTransformationActorsComponent::BeginPlay()
{


}

void UTransformationActorsComponent::StartTransformationActor()
{
	if (GetIsTransform() || GetTransformState() == ETransformState::ETS_Idle)
	{
		return;
	}

	AActor* FoundActor = FindActorUnderCursor();

	if (FoundActor == nullptr)
	{
		if (bIsShowDebugMessages)
		{
			UE_LOG(LogTemp, Warning, TEXT("TransformationActors: StartTransformationActor(): FoundActor is not valid."));
		}
		return;
	}
	if (!CheckActorOnTransformationActorsInterface(FoundActor))
	{
		return;
	}

	SumInputAxisValue = 0.f;

	if (FoundActor == GetPreviousTransformActor())
	{
		StartTransformTimer(GetTransformState());
	}

	if (FoundActor != GetPreviousTransformActor())
	{
		SelectNewTransformActor(FoundActor);
	}
}

void UTransformationActorsComponent::StopTransformationActor()
{
	if (GetTransformState() != ETransformState::ETS_Idle)
	{
		OnStopTransformationActor.Broadcast();
		StopTransformation_TransformationActorsInterface(GetTransformActor());
		SetIsTransform(false);
	}
	if (GetTransformState() == ETransformState::ETS_Location)
	{
		SetIsLockFirstIterationLocationTimer(false);
		StopLocationTimer();
	}
	if (GetTransformState() == ETransformState::ETS_Rotation_YawPitch
		|| GetTransformState() == ETransformState::ETS_Rotation_Roll
		|| GetTransformState() == ETransformState::ETS_Rotation_Pitch
		|| GetTransformState() == ETransformState::ETS_Rotation_Yaw
		)
	{
		SetIsLockFirstIterationRotationTimer(false);
		StopRotationTimer();
	}
	if (GetTransformState() == ETransformState::ETS_Scale)
	{
		SetIsLockFirstIterationScaleTimer(false);
		StopScaleTimer();
	}
}


void UTransformationActorsComponent::SwitchOnTransformationMode(ETransformState InTransformState)
{
	if (
		!SpecifyControllerAndPawn()
		|| GetIsTransform() 
		|| InTransformState == ETransformState::ETS_Idle 
		|| GetTransformState() == InTransformState
		)
	{
		return;
	}

	SetInputModeGameAndUI();
	SetTransformState(InTransformState);
	OnSwitchOnTransformationMode.Broadcast();
}

void UTransformationActorsComponent::SwitchOffTransformationMode()
{
	if (GetTransformState() == ETransformState::ETS_Idle)
	{
		return;
	}

	SetInputModeGameOnly();

	if (GetIsTransform())
	{
		StopTransformationActor();
	}

	ResetTransform();
	OnSwitchOffTransformationMode.Broadcast();
}


void UTransformationActorsComponent::CalcSumInputAxisValue(float InputAxisValue)
{
	if (!GetIsTransform())
	{
		return;
	}
	SumInputAxisValue += InputAxisValue;
}

void UTransformationActorsComponent::LocationLeftRightKeyboard(float AxisValue)
{
	if (AxisValue == 0.f)
	{
		return;
	}

	FVector DeltaLocation = FVector(0.f, AxisValue * LocationSpeedKeyboard, 0.f);

	LocationKeyboardBasic(DeltaLocation);

}

void UTransformationActorsComponent::LocationUpDownKeyboard(float AxisValue)
{
	if (AxisValue == 0.f)
	{
		return;
	}

	FVector DeltaLocation = FVector(0.f, 0.f, AxisValue * LocationSpeedKeyboard);

	LocationKeyboardBasic(DeltaLocation);

}

void UTransformationActorsComponent::LocationInsideOutsideKeyboard(float AxisValue)
{
	if (AxisValue == 0.f)
	{
		return;
	}

	FVector DeltaLocation = FVector(AxisValue * LocationSpeedKeyboard, 0.f, 0.f);

	LocationKeyboardBasic(DeltaLocation);
}

void UTransformationActorsComponent::RotationRollKeyboard(float AxisValue)
{
	if (AxisValue == 0.f)
	{
		return;
	}

	if (GetPlayerPawn() == nullptr)
	{
		if (bIsShowDebugMessages)
		{
			UE_LOG(LogTemp, Warning, TEXT("TransformationActors: RotationRollKeyboard(): PlayerPawn is not valid."));
		}
		return;
	}

	FVector AxeRoll;

	if (GetComponentForTransformationAxis())
	{
		AxeRoll = GetComponentForTransformationAxis()->GetForwardVector();
	}
	else
	{
		AxeRoll = GetPlayerPawn()->GetActorForwardVector();
	}

	RotationKeyboardBasic(AxisValue, AxeRoll);
}

void UTransformationActorsComponent::RotationPitchKeyboard(float AxisValue)
{
	if (AxisValue == 0.f)
	{
		return;
	}

	if (GetPlayerPawn() == nullptr)
	{
		if (bIsShowDebugMessages)
		{
			UE_LOG(LogTemp, Warning, TEXT("TransformationActors: RotationRollKeyboard(): PlayerPawn is not valid."));
		}
		return;
	}

	FVector	AxePitch;

	if (GetComponentForTransformationAxis())
	{
		AxePitch = GetComponentForTransformationAxis()->GetRightVector();
	}
	else
	{
		AxePitch = GetPlayerPawn()->GetActorRightVector();
	}

	RotationKeyboardBasic(AxisValue, AxePitch);
}

void UTransformationActorsComponent::RotationYawKeyboard(float AxisValue)
{
	if (AxisValue == 0.f)
	{
		return;
	}

	if (GetPlayerPawn() == nullptr)
	{
		if (bIsShowDebugMessages)
		{
			UE_LOG(LogTemp, Warning, TEXT("TransformationActors: RotationRollKeyboard(): PlayerPawn is not valid."));
		}
		return;
	}

	FVector	AxeYaw;

	if (GetComponentForTransformationAxis())
	{
		AxeYaw = GetComponentForTransformationAxis()->GetUpVector();
	}
	else
	{
		AxeYaw = GetPlayerPawn()->GetActorUpVector();
	}

	RotationKeyboardBasic(AxisValue, AxeYaw);
}

void UTransformationActorsComponent::ScaleKeyboard(float AxisValue)
{
	if (AxisValue == 0.f)
	{
		return;
	}

	FVector DeltaScale3D = FVector(AxisValue * ScaleSpeedKeyboard);

	ScaleKeyboardBasic(DeltaScale3D);
}

void UTransformationActorsComponent::ScaleXKeyboard(float AxisValue)
{
	if (AxisValue == 0.f)
	{
		return;
	}

	FVector DeltaScaleX = FVector(AxisValue * ScaleSpeedKeyboard, 0.f, 0.f);

	ScaleKeyboardBasic(DeltaScaleX);
}

void UTransformationActorsComponent::ScaleYKeyboard(float AxisValue)
{
	if (AxisValue == 0.f)
	{
		return;
	}

	FVector DeltaScaleY = FVector(0.f, AxisValue * ScaleSpeedKeyboard, 0.f);

	ScaleKeyboardBasic(DeltaScaleY);
}

void UTransformationActorsComponent::ScaleZKeyboard(float AxisValue)
{
	if (AxisValue == 0.f)
	{
		return;
	}

	FVector DeltaScaleZ = FVector(0.f, 0.f, AxisValue * ScaleSpeedKeyboard);

	ScaleKeyboardBasic(DeltaScaleZ);
}

void UTransformationActorsComponent::LocationKeyboardBasic(FVector DeltaLocation)
{
	if (GetTransformActor() == nullptr)
	{
		if (bIsShowDebugMessages)
		{
			UE_LOG(LogTemp, Warning, TEXT("TransformationActors: LocationKeyboard(): TransformActor is not valid."));
		}
		return;
	}

	if (GetPlayerPawn() == nullptr)
	{
		if (bIsShowDebugMessages)
		{
			UE_LOG(LogTemp, Warning, TEXT("TransformationActors: LocationKeyboard(): PlayerPawn is not valid."));
		}
		return;
	}

	FTransform ComponentAxisTransform;

	if (GetComponentForTransformationAxis())
	{
		ComponentAxisTransform = GetComponentForTransformationAxis()->GetComponentTransform();
	}
	else
	{
		ComponentAxisTransform = GetPlayerPawn()->GetRootComponent()->GetComponentTransform();
	}

	FVector CurrentLocation = GetTransformActor()->GetActorLocation();

	FVector CurrenLocationInComponentSpace = UKismetMathLibrary::InverseTransformLocation(ComponentAxisTransform, CurrentLocation);

	FVector NewLocationInComponentSpace = CurrenLocationInComponentSpace + DeltaLocation;

	FTransform NewTransformInComponentSpace = FTransform(NewLocationInComponentSpace);

	FTransform NewTransform = UKismetMathLibrary::ComposeTransforms(NewTransformInComponentSpace, ComponentAxisTransform);

	FVector NewLocation = NewTransform.GetTranslation();

	GetTransformActor()->SetActorLocation(NewLocation, bSweep);

}

void UTransformationActorsComponent::RotationKeyboardBasic(float AxisValue, FVector Axe)
{
	if (GetTransformActor() == nullptr)
	{
		if (bIsShowDebugMessages)
		{
			UE_LOG(LogTemp, Warning, TEXT("TransformationActors: RotationKeyboard(): TransformActor is not valid."));
		}
		return;
	}

	float DeltaDegree = AxisValue * RotationSpeedKeyboard;
	float DeltaRadian = FMath::DegreesToRadians(DeltaDegree);

	FQuat DeltaRotationQ = FQuat(Axe, DeltaRadian);

	GetTransformActor()->AddActorWorldRotation(DeltaRotationQ, bSweep);
}

void UTransformationActorsComponent::ScaleKeyboardBasic(FVector DeltaScale3D)
{
	if (GetTransformActor() == nullptr)
	{
		if (bIsShowDebugMessages)
		{
			UE_LOG(LogTemp, Warning, TEXT("TransformationActors: ScaleKeyboardBasic(): TransformActor is not valid."));
		}
		return;
	}

	FVector CurrentScale3D = GetTransformActor()->GetActorScale3D();

	FVector NewScale3DKeyboard = CurrentScale3D + DeltaScale3D;

	/*Limit the minimum scale.*/
	if (NewScale3DKeyboard.X <= MinScale || NewScale3DKeyboard.Y <= MinScale || NewScale3DKeyboard.Z <= MinScale)
	{
		NewScale3DKeyboard = CurrentScale3D;
	}

	GetTransformActor()->SetActorScale3D(NewScale3DKeyboard);

}

void UTransformationActorsComponent::SetInputModeGameAndUI()
{
	if (GetPlayerController())
	{
		FInputModeGameAndUI InputModeGameAndUI;
		InputModeGameAndUI.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		InputModeGameAndUI.SetHideCursorDuringCapture(false);
		GetPlayerController()->SetInputMode(InputModeGameAndUI);
		GetPlayerController()->bShowMouseCursor = true;
	}
	else if (bIsShowDebugMessages)
	{
		UE_LOG(LogTemp, Warning, TEXT("TransformationActors: SetInputModeGameAndUI(): PlayerController is not valid."));
	}

}

void UTransformationActorsComponent::SetInputModeGameOnly()
{
	if (GetPlayerController())
	{
		FInputModeGameOnly InputModeGameOnly;
		InputModeGameOnly.SetConsumeCaptureMouseDown(false);
		GetPlayerController()->SetInputMode(InputModeGameOnly);
		GetPlayerController()->bShowMouseCursor = false;
	}
	else if (bIsShowDebugMessages)
	{
		UE_LOG(LogTemp, Warning, TEXT("TransformationActors: SetInputModeGameOnly(): PlayerController is not valid."));
	}

}

void UTransformationActorsComponent::SetInputModeUIOnly()
{
	if (GetPlayerController())
	{
		FInputModeUIOnly InputModeUIOnly;
		InputModeUIOnly.SetWidgetToFocus(nullptr);
		InputModeUIOnly.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		GetPlayerController()->SetInputMode(InputModeUIOnly);
		GetPlayerController()->bShowMouseCursor = true;
	}
	else if (bIsShowDebugMessages)
	{
		UE_LOG(LogTemp, Warning, TEXT("TransformationActors: SetInputModeUIOnly(): PlayerController is not valid."));
	}
}

AActor* UTransformationActorsComponent::FindActorUnderCursor()
{
	if (GetPlayerController() == nullptr)
	{
		if (bIsShowDebugMessages)
		{
			UE_LOG(LogTemp, Warning, TEXT("TransformationActors: FindActorUnderCursor(): PlayerController is not valid."));
		}
		return nullptr;
	}

	FHitResult HitResult;
	if (GetPlayerController()->GetHitResultUnderCursor(ECC_Visibility, true, HitResult))
	{
		return HitResult.GetActor();
	}
	else
	{
		if (bIsShowDebugMessages)
		{
			UE_LOG(LogTemp, Warning, TEXT("TransformationActors: FindActorUnderCursor(): GetPlayerController()->GetHitResultUnderCursor(ECC_Visibility, true, HitResult) return false."));
		}
		return nullptr;
	}

}

void UTransformationActorsComponent::StartTransformTimer(ETransformState CurrentTransformState)
{
	if (CurrentTransformState != ETransformState::ETS_Idle)
	{
		SetIsTransform(true);
		OnStartTransformationActor.Broadcast();
		StartTransformation_TransformationActorsInterface(GetTransformActor());
	}
	if (CurrentTransformState == ETransformState::ETS_Location)
	{
		SetIsLockFirstIterationLocationTimer(false);
		StartLocationTimer();
	}
	if (CurrentTransformState == ETransformState::ETS_Rotation_YawPitch
		|| CurrentTransformState == ETransformState::ETS_Rotation_Roll
		|| CurrentTransformState == ETransformState::ETS_Rotation_Pitch
		|| CurrentTransformState == ETransformState::ETS_Rotation_Yaw)
	{
		SetIsLockFirstIterationRotationTimer(false);
		StartRotationTimer(CurrentTransformState);
	}
	if (CurrentTransformState == ETransformState::ETS_Scale)
	{
		SetIsLockFirstIterationScaleTimer(false);
		StartScaleTimer();
	}
}

void UTransformationActorsComponent::StartLocationTimer()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(LocationTimer, this, &UTransformationActorsComponent::LocationActor, LocationTimerDeltaTime, true);
	}
	else if (bIsShowDebugMessages)
	{
		UE_LOG(LogTemp, Warning, TEXT("TransformationActors: StartLocationTimer(): GetWorld() is not valid."));
	}

}

void UTransformationActorsComponent::StartRotationTimer(ETransformState CurrentTransformState)
{
	if (GetWorld() == nullptr)
	{
		if (bIsShowDebugMessages)
		{
			UE_LOG(LogTemp, Warning, TEXT("TransformationActors: StartRotationTimer(): GetWorld() is not valid."));
		}
		return;
	}

	GetWorld()->GetTimerManager().SetTimer(RotationTimer, this, &UTransformationActorsComponent::RotationActor, RotationTimerDeltaTime, true);

}

void UTransformationActorsComponent::StartScaleTimer()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(ScaleTimer, this, &UTransformationActorsComponent::ScaleActor, ScaleTimerDeltaTime, true);
	}
	else if (bIsShowDebugMessages)
	{
		UE_LOG(LogTemp, Warning, TEXT("TransformationActors: StartScaleTimer(): GetWorld() is not valid."));
	}

}

void UTransformationActorsComponent::LocationActor()
{
	if (GetPlayerController() == nullptr)
	{
		if (bIsShowDebugMessages)
		{
			UE_LOG(LogTemp, Warning, TEXT("TransformationActors: LocationActor(): PlayerController is not valid."));
		}
		return;
	}

	FVector
		/*New position of TransformActor, which will be calculated based on the coordinates of the cursor.*/
		NewLocation,
		/*Cursor position in world coordinates.*/
		WorldLocation,
		/*Cursor direction in world coordinates.*/
		WorldDirection;

	
	/*Translate cursor coordinates to world coordinates.*/
	if (!GetPlayerController()->DeprojectMousePositionToWorld(WorldLocation, WorldDirection))
	{
		if (bIsShowDebugMessages)
		{
			UE_LOG(LogTemp, Warning, TEXT("TransformationActors: LocationActor(): GetPlayerController()->DeprojectMousePositionToWorld(WorldLocation, WorldDirection) return false."));
		}
		return;
	}

	/*Set the distance in the first tick.*/
	if (!GetIsLockFirstIterationLocationTimer())
	{
		/*Block change of distance from mouse cursor to TransformActor,
		if the movement is in the plane of the screen.
		*/
		DistanceToCursorSave = FVector::Distance(GetTransformActor()->GetActorLocation(), WorldLocation);

		SetIsLockFirstIterationLocationTimer(true);
	}

	float MultiplierDistance = DistanceToCursorSave + (SumInputAxisValue * LocationDeepSpeed);

	NewLocation = WorldLocation + (WorldDirection * MultiplierDistance);

	/*Slightly removes jerking when moving, but the actor lags behind the cursor.*/
	FVector InterpNewLocation = FMath::VInterpTo(GetTransformActor()->GetActorLocation(), NewLocation, LocationTimerDeltaTime, LocationSpeed);

	//UE_LOG(LogTemp, Warning, TEXT("Roll: %f, Pitch: %f, Yaw: %f"), Rotation.Roll, Rotation.Pitch, Rotation.Yaw);

	GetTransformActor()->SetActorLocation(InterpNewLocation, bSweep);

}

void UTransformationActorsComponent::RotationActor()
{
	if (!CheckControllerAndPawn())
	{
		return;
	}

	float
		/*The current coordinates of the mouse.*/
		LocationX,
		LocationY;

	if (!GetPlayerController()->GetMousePosition(LocationX, LocationY))
	{
		if (bIsShowDebugMessages)
		{
			UE_LOG(LogTemp, Warning, TEXT("TransformationActors: RotationActor(): GetPlayerController()->GetMousePosition(LocationX, LocationY) return false."));
		}
		return;
	}
	
	/*Actions when you click on an actor. Performed in the first tick of the timer after each click.*/
	if (!GetIsLockFirstIterationRotationTimer())
	{
		/*
		Remember the initial rotation angles from which changes in angles will be calculated.
		If you do not remember, the corners from the previous click will be counted and the actor will rotate sharply.
		*/
		RollSave = LocationX;
		PitchSave = LocationY;
		YawSave = LocationX;

		SetIsLockFirstIterationRotationTimer(true);

	}

	CalcDeltaRoll(LocationX);
	CalcDeltaPitch(LocationY);
	CalcDeltaYaw(LocationX);

	float
		DeltaRollDegreeTmp = GetDeltaRollDegree() * RotationSpeed,
		DeltaPitchDegreeTmp = GetDeltaPitchDegree() * RotationSpeed,
		DeltaYawDegreeTmp = GetDeltaYawDegree() * RotationSpeed;
	float
		DeltaRollRadian = FMath::DegreesToRadians(DeltaRollDegreeTmp),
		DeltaPitchRadian = FMath::DegreesToRadians(DeltaPitchDegreeTmp),
		DeltaYawRadian = FMath::DegreesToRadians(DeltaYawDegreeTmp);

	FQuat
		DeltaRotationQRoll,
		DeltaRotationQPitch,
		DeltaRotationQYaw,
		DeltaRotationQ;

	FVector
		AxeRoll,
		AxePitch,
		AxeYaw;

	if (GetComponentForTransformationAxis())
	{
		AxeRoll = -GetComponentForTransformationAxis()->GetForwardVector();
		AxePitch = -GetComponentForTransformationAxis()->GetRightVector();
		AxeYaw = -GetComponentForTransformationAxis()->GetUpVector();
	}
	else
	{
		AxeRoll = -GetPlayerPawn()->GetActorForwardVector();
		AxePitch = -GetPlayerPawn()->GetActorRightVector();
		AxeYaw = -GetPlayerPawn()->GetActorUpVector();
	}

	DeltaRotationQRoll = FQuat(AxeRoll, DeltaRollRadian);
	DeltaRotationQPitch = FQuat(AxePitch, DeltaPitchRadian);
	DeltaRotationQYaw = FQuat(AxeYaw, DeltaYawRadian);

	switch (GetTransformState())
	{
	case ETransformState::ETS_Rotation_Roll :
		DeltaRotationQ = DeltaRotationQRoll;
		break;
	case ETransformState::ETS_Rotation_Pitch :
		DeltaRotationQ = DeltaRotationQPitch;
		break;
	case ETransformState::ETS_Rotation_Yaw :
		DeltaRotationQ = DeltaRotationQYaw;
		break;
	case ETransformState::ETS_Rotation_YawPitch :
		DeltaRotationQ = DeltaRotationQPitch * DeltaRotationQYaw;
		break;
	default:
		return;
	}

	GetTransformActor()->AddActorWorldRotation(DeltaRotationQ, bSweep);

}


void UTransformationActorsComponent::ScaleActor()
{
	if (GetPlayerController() == nullptr)
	{
		if (bIsShowDebugMessages)
		{
			UE_LOG(LogTemp, Warning, TEXT("TransformationActors: ScaleActor(): PlayerController is not valid."));
		}
		return;
	}

	float
		/*The current coordinates of the mouse.*/
		LocationX,
		LocationY,
		/*The difference between the current and the first coordinates of the mouse.*/
		DeltaLocationX,
		DeltaLocationY,
		/*Mouse path length in 2D coordinates. The larger the DeltaLocation, the larger the scale.*/
		DeltaLocationXY;

	if (!GetPlayerController()->GetMousePosition(LocationX, LocationY))
	{
		if (bIsShowDebugMessages)
		{
			UE_LOG(LogTemp, Warning, TEXT("TransformationActors: ScaleActor(): GetPlayerController()->GetMousePosition(LocationX, LocationY) return false."));
		}
		return;
	}

	//UE_LOG(LogTemp, Warning, TEXT("LocationX: %f, LocationY: %f"), LocationX, LocationY);

	/*Set initial mouse coordinates and scale.*/
	if (!GetIsLockFirstIterationScaleTimer())
	{
		LocationXAtClick = LocationX;
		LocationYAtClick = LocationY;
		Scale3DSave = GetTransformActor()->GetActorScale3D();
		SetIsLockFirstIterationScaleTimer(true);
	}

	DeltaLocationX = LocationX - LocationXAtClick;
	DeltaLocationY = LocationY - LocationYAtClick;
	DeltaLocationXY = FMath::Sqrt(FMath::Square(FMath::Abs(DeltaLocationX)) + FMath::Square(FMath::Abs(DeltaLocationY)));

	/*If you move the cursor above the click point, increase the scale.*/
	if (LocationY < LocationYAtClick)
	{
		NewScale3D = Scale3DSave + (DeltaLocationXY * ScaleSpeed);
	}
	/*If you move the cursor below a click point, decrease the scale.*/
	if (LocationY > LocationYAtClick)
	{
		FVector NewScale3DTmp = NewScale3D;
		NewScale3D = Scale3DSave - (DeltaLocationXY * ScaleSpeed);

		/*Limit the minimum scale.*/
		if (NewScale3D.X <= MinScale || NewScale3D.Y <= MinScale || NewScale3D.Z <= MinScale)
		{
			NewScale3D = NewScale3DTmp;
		}

	}

	if (LocationY == LocationYAtClick)
	{
		NewScale3D = Scale3DSave;
	}

	GetTransformActor()->SetActorScale3D(NewScale3D);


}

void UTransformationActorsComponent::StopLocationTimer()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(LocationTimer);
	}
	else if (bIsShowDebugMessages)
	{
		UE_LOG(LogTemp, Warning, TEXT("TransformationActors: StopLocationTimer(): GetWorld() is not valid."));
	}
}

void UTransformationActorsComponent::StopRotationTimer()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(RotationTimer);
	}
	else if (bIsShowDebugMessages)
	{
		UE_LOG(LogTemp, Warning, TEXT("TransformationActors: StopRotationTimer(): GetWorld() is not valid."));
	}
}

void UTransformationActorsComponent::StopScaleTimer()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(ScaleTimer);
	}
	else if (bIsShowDebugMessages)
	{
		UE_LOG(LogTemp, Warning, TEXT("TransformationActors: StopScaleTimer(): GetWorld() is not valid."));
	}
}

void UTransformationActorsComponent::SelectNewTransformActor(AActor* NewTransformActor)
{
	HighlightOff_TransformationActorsInterface(GetPreviousTransformActor());
	HighlightOn_TransformationActorsInterface(NewTransformActor);
	SetPreviousTransformActor(NewTransformActor);
	SetTransformActor(NewTransformActor);
}

void UTransformationActorsComponent::HighlightOn_TransformationActorsInterface(AActor* Actor)
{
	if (Actor == nullptr)
	{
		if (bIsShowDebugMessages)
		{
			UE_LOG(LogTemp, Warning, TEXT("TransformationActors: HighlightOn_TransformationActorsInterface(AActor* Actor): Actor is not valid."));
		}
		return;
	}

	if (Actor->GetClass()->ImplementsInterface(UTransformationActorsInterface::StaticClass()))
	{
		ITransformationActorsInterface::Execute_HighlightOn(Actor);
	}
}

void UTransformationActorsComponent::HighlightOff_TransformationActorsInterface(AActor* Actor)
{
	if (Actor == nullptr)
	{
		if (bIsShowDebugMessages)
		{
			UE_LOG(LogTemp, Warning, TEXT("TransformationActors: HighlightOff_TransformationActorsInterface(AActor* Actor): Actor is not valid."));
		}
		return;
	}

	if (Actor->GetClass()->ImplementsInterface(UTransformationActorsInterface::StaticClass()))
	{
		ITransformationActorsInterface::Execute_HighlightOff(Actor);
	}
}

void UTransformationActorsComponent::StartTransformation_TransformationActorsInterface(AActor* Actor)
{
	if (Actor == nullptr)
	{
		if (bIsShowDebugMessages)
		{
			UE_LOG(LogTemp, Warning, TEXT("TransformationActors: StartTransformation_TransformationActorsInterface(AActor* Actor): Actor is not valid."));
		}
		return;
	}

	if (Actor->GetClass()->ImplementsInterface(UTransformationActorsInterface::StaticClass()))
	{
		ITransformationActorsInterface::Execute_StartTransformation(Actor);
	}

}

void UTransformationActorsComponent::StopTransformation_TransformationActorsInterface(AActor* Actor)
{
	if (Actor == nullptr)
	{
		if (bIsShowDebugMessages)
		{
			UE_LOG(LogTemp, Warning, TEXT("TransformationActors: StopTransformation_TransformationActorsInterface(AActor* Actor): Actor is not valid."));
		}
		return;
	}

	if (Actor->GetClass()->ImplementsInterface(UTransformationActorsInterface::StaticClass()))
	{
		ITransformationActorsInterface::Execute_StopTransformation(Actor);
	}

}

bool UTransformationActorsComponent::CheckActorOnTransformationActorsInterface(AActor* Actor)
{
	if (Actor == nullptr)
	{
		if (bIsShowDebugMessages)
		{
			UE_LOG(LogTemp, Warning, TEXT("TransformationActors: CheckActorOnTransformationActorsInterface(AActor* Actor): Actor is not valid."));
		}
		return false;
	}

	if (Actor->GetClass()->ImplementsInterface(UTransformationActorsInterface::StaticClass()))
	{
		return true;
	}
	else
	{
		if (bIsShowDebugMessages)
		{
			UE_LOG(LogTemp, Warning, TEXT("TransformationActors: CheckActorOnTransformationActorsInterface(AActor* Actor): Actor %s is not implements TransformationActorsInterface."), *Actor->GetName());
		}
		return false;
	}
}

void UTransformationActorsComponent::ResetTransform()
{
	HighlightOff_TransformationActorsInterface(GetTransformActor());
	SetPreviousTransformActor(nullptr);
	SetTransformActor(nullptr);
	SetTransformState(ETransformState::ETS_Idle);
}

void UTransformationActorsComponent::CalcDeltaRoll(float Roll)
{
	DeltaRollDegree = Roll - RollSave;
	RollSave = Roll;
}

void UTransformationActorsComponent::CalcDeltaPitch(float Pitch)
{
	DeltaPitchDegree = Pitch - PitchSave;
	PitchSave = Pitch;
}

void UTransformationActorsComponent::CalcDeltaYaw(float Yaw)
{
	DeltaYawDegree = Yaw - YawSave;
	YawSave = Yaw;
}

bool UTransformationActorsComponent::SpecifyControllerAndPawn()
{
	bool bAllValid = true;

	UWorld* World = GetWorld();
	if (World)
	{
		SetPlayerController(UGameplayStatics::GetPlayerController(World, 0));
		if (GetPlayerController())
		{
			SetPlayerPawn(GetPlayerController()->GetPawnOrSpectator());
			if (GetPlayerPawn() == nullptr)
			{
				if (bIsShowDebugMessages)
				{
					UE_LOG(LogTemp, Warning, TEXT("TransformationActors: SpecifyControllerAndPawn(): PlayerPawn is not valid."));
				}
				bAllValid = false;
			}
		}
		else
		{
			if (bIsShowDebugMessages)
			{
				UE_LOG(LogTemp, Warning, TEXT("TransformationActors: SpecifyControllerAndPawn(): PlayerController is not valid."));
			}
			bAllValid = false;
		}
	}
	else
	{
		if (bIsShowDebugMessages)
		{
			UE_LOG(LogTemp, Warning, TEXT("TransformationActors: SpecifyControllerAndPawn(): GetWorld() is not valid."));
		}
		bAllValid = false;
	}

	return bAllValid;

}

bool UTransformationActorsComponent::CheckControllerAndPawn()
{
	bool bAllValid = true;

	if (GetPlayerController() == nullptr)
	{
		if (bIsShowDebugMessages)
		{
			UE_LOG(LogTemp, Warning, TEXT("TransformationActors: CheckControllerAndPawn(): PlayerController is not valid."));
		}
		bAllValid = false;
	}
	if (GetPlayerPawn() == nullptr)
	{
		if (bIsShowDebugMessages)
		{
			UE_LOG(LogTemp, Warning, TEXT("TransformationActors: CheckControllerAndPawn(): PlayerPawn is not valid."));
		}
		bAllValid = false;
	}

	return bAllValid;
}



