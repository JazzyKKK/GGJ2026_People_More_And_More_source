#include "PartyExperiment/GGJPartyPlayerController.h"

#include "Engine/World.h"
#include "PartyExperiment/GGJPartyGameMode.h"

AGGJPartyPlayerController::AGGJPartyPlayerController()
{
    PrimaryActorTick.bCanEverTick = true;

    FGGJPartyKeyboardControlScheme PlayerOne;
    PlayerOne.PlayerIndex = 0;
    PlayerOne.MoveUpKey = EKeys::W;
    PlayerOne.MoveDownKey = EKeys::S;
    PlayerOne.MoveLeftKey = EKeys::A;
    PlayerOne.MoveRightKey = EKeys::D;
    KeyboardControlSchemes.Add(PlayerOne);

    FGGJPartyKeyboardControlScheme PlayerTwo;
    PlayerTwo.PlayerIndex = 1;
    PlayerTwo.MoveUpKey = EKeys::Up;
    PlayerTwo.MoveDownKey = EKeys::Down;
    PlayerTwo.MoveLeftKey = EKeys::Left;
    PlayerTwo.MoveRightKey = EKeys::Right;
    KeyboardControlSchemes.Add(PlayerTwo);
}

void AGGJPartyPlayerController::BeginPlay()
{
    Super::BeginPlay();
    bShowMouseCursor = false;
    FInputModeGameOnly InputMode;
    SetInputMode(InputMode);
}

void AGGJPartyPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();
    if (!InputComponent)
    {
        return;
    }

    // 连续方向在 PlayerTick 中读取；这里只绑定有明确按下边沿的共享相机旋转。
    InputComponent->BindKey(RotateCameraLeftKey, IE_Pressed, this,
        &AGGJPartyPlayerController::RotateCameraLeft);
    InputComponent->BindKey(RotateCameraRightKey, IE_Pressed, this,
        &AGGJPartyPlayerController::RotateCameraRight);
}

void AGGJPartyPlayerController::PlayerTick(const float DeltaTime)
{
    Super::PlayerTick(DeltaTime);
    AGGJPartyGameMode* PartyGameMode = GetWorld()
        ? GetWorld()->GetAuthGameMode<AGGJPartyGameMode>() : nullptr;
    if (!PartyGameMode)
    {
        return;
    }

    for (const FGGJPartyKeyboardControlScheme& Scheme : KeyboardControlSchemes)
    {
        if (!Scheme.bEnabled)
        {
            continue;
        }
        const FVector2D ScreenMovement(
            ReadDigitalAxis(Scheme.MoveRightKey, Scheme.MoveLeftKey),
            ReadDigitalAxis(Scheme.MoveUpKey, Scheme.MoveDownKey));
        PartyGameMode->ApplyPlayerScreenMovement(Scheme.PlayerIndex,
            ScreenMovement.GetClampedToMaxSize(1.f));
    }
}

float AGGJPartyPlayerController::ReadDigitalAxis(const FKey& PositiveKey,
    const FKey& NegativeKey) const
{
    const float Positive = IsInputKeyDown(PositiveKey) ? 1.f : 0.f;
    const float Negative = IsInputKeyDown(NegativeKey) ? 1.f : 0.f;
    return Positive - Negative;
}

void AGGJPartyPlayerController::RotateCameraLeft()
{
    if (AGGJPartyGameMode* PartyGameMode = GetWorld()
        ? GetWorld()->GetAuthGameMode<AGGJPartyGameMode>() : nullptr)
    {
        PartyGameMode->RequestSharedCameraTurn(1);
    }
}

void AGGJPartyPlayerController::RotateCameraRight()
{
    if (AGGJPartyGameMode* PartyGameMode = GetWorld()
        ? GetWorld()->GetAuthGameMode<AGGJPartyGameMode>() : nullptr)
    {
        PartyGameMode->RequestSharedCameraTurn(-1);
    }
}
