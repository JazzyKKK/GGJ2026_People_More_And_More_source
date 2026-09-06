// Enhanced Input、Q/E 转镜和全体共享移动方向的汇合实现。
// 输入先按相机转换为世界方向，再由 PopulationSubsystem 发布给所有小人。
#include "Crowd/Framework/CrowdPlayerController.h"
#include "Crowd/Framework/CrowdCameraPawn.h"
#include "Crowd/Framework/CrowdLevelDirector.h"
#include "Crowd/Core/CrowdPopulationSubsystem.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"

void ACrowdPlayerController::EnsureInputAssets()
{
    // 保留已有自定义移动映射；只缺 Q/E 时另建独立兜底 Context，不污染用户资产。
    if (!OrbitMapping && (!RotateLeftAction || !RotateRightAction))
    {
        OrbitMapping = NewObject<UInputMappingContext>(this, TEXT("IMC_CrowdOrbit_Runtime"));
        if (!RotateLeftAction)
        {
            RotateLeftAction = NewObject<UInputAction>(this, TEXT("IA_CrowdRotateLeft_Runtime"));
            RotateLeftAction->ValueType = EInputActionValueType::Boolean;
            OrbitMapping->MapKey(RotateLeftAction, EKeys::Q);
        }
        if (!RotateRightAction)
        {
            RotateRightAction = NewObject<UInputAction>(this, TEXT("IA_CrowdRotateRight_Runtime"));
            RotateRightAction->ValueType = EInputActionValueType::Boolean;
            OrbitMapping->MapKey(RotateRightAction, EKeys::E);
        }
    }
    // InputMapping、Move、Restart 三者齐全即采用用户配置；否则整套生成，避免半套引用不一致。
    if (InputMapping && MoveAction && RestartAction) { return; }
    InputMapping = NewObject<UInputMappingContext>(this, TEXT("IMC_Crowd_Runtime"));
    MoveAction = NewObject<UInputAction>(this, TEXT("IA_CrowdMove_Runtime"));
    MoveAction->ValueType = EInputActionValueType::Axis2D;
    MoveAction->AccumulationBehavior = EInputActionAccumulationBehavior::Cumulative;
    RestartAction = NewObject<UInputAction>(this, TEXT("IA_CrowdRestart_Runtime"));
    RestartAction->ValueType = EInputActionValueType::Boolean;

    // Enhanced Input 的键盘单键初始落在 X；Negate 负责反向，Swizzle 把 X 交换到 Y。
    auto MapDirection = [this](FKey Key, bool bNegative, bool bY)
    {
        FEnhancedActionKeyMapping& Mapping = InputMapping->MapKey(MoveAction, Key);
        if (bNegative)
        {
            Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(InputMapping));
        }
        if (bY)
        {
            UInputModifierSwizzleAxis* Swizzle = NewObject<UInputModifierSwizzleAxis>(InputMapping);
            Swizzle->Order = EInputAxisSwizzle::YXZ;
            Mapping.Modifiers.Add(Swizzle);
        }
    };
    MapDirection(EKeys::D, false, false);
    MapDirection(EKeys::A, true, false);
    MapDirection(EKeys::W, false, true);
    MapDirection(EKeys::S, true, true);
    MapDirection(EKeys::Right, false, false);
    MapDirection(EKeys::Left, true, false);
    MapDirection(EKeys::Up, false, true);
    MapDirection(EKeys::Down, true, true);
    // 手柄直接产生 Axis2D，DeadZone 过滤摇杆中心漂移。
    FEnhancedActionKeyMapping& Stick = InputMapping->MapKey(MoveAction, EKeys::Gamepad_Left2D);
    Stick.Modifiers.Add(NewObject<UInputModifierDeadZone>(InputMapping));
    InputMapping->MapKey(RestartAction, EKeys::R);
}

void ACrowdPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();
    EnsureInputAssets();
    if (UEnhancedInputComponent* Enhanced = Cast<UEnhancedInputComponent>(InputComponent))
    {
        // Triggered 每帧更新轴；Completed/Canceled 都清零，防止松键或输入被系统取消后继续走。
        Enhanced->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ACrowdPlayerController::HandleMove);
        Enhanced->BindAction(MoveAction, ETriggerEvent::Completed, this, &ACrowdPlayerController::ClearMove);
        Enhanced->BindAction(MoveAction, ETriggerEvent::Canceled, this, &ACrowdPlayerController::ClearMove);
        Enhanced->BindAction(RestartAction, ETriggerEvent::Started, this, &ACrowdPlayerController::Restart);
        Enhanced->BindAction(RotateLeftAction, ETriggerEvent::Started, this, &ACrowdPlayerController::RotateLeft);
        Enhanced->BindAction(RotateRightAction, ETriggerEvent::Started, this, &ACrowdPlayerController::RotateRight);
    }
}

void ACrowdPlayerController::BeginPlay()
{
    Super::BeginPlay();
    EnsureInputAssets();
    if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
    {
        if (UEnhancedInputLocalPlayerSubsystem* Input = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
        {
            // 主 Context 使用优先级 0；独立 Q/E 兜底使用 1，确保缺失 Action 仍能触发。
            Input->AddMappingContext(InputMapping, 0);
            if (OrbitMapping) { Input->AddMappingContext(OrbitMapping, 1); }
        }
    }
    SetInputMode(FInputModeGameOnly());
    bShowMouseCursor = false;
}

void ACrowdPlayerController::HandleMove(const FInputActionValue& Value)
{
    CurrentScreenInput = Value.Get<FVector2D>();
}

void ACrowdPlayerController::ClearMove(const FInputActionValue& Value)
{
    CurrentScreenInput = FVector2D::ZeroVector;
}

void ACrowdPlayerController::PostProcessInput(const float DeltaTime, const bool bGamePaused)
{
    Super::PostProcessInput(DeltaTime, bGamePaused);
    if (UCrowdPopulationSubsystem* Population = GetWorld()->GetSubsystem<UCrowdPopulationSubsystem>())
    {
        ACrowdCameraPawn* CameraPawn = Cast<ACrowdCameraPawn>(GetPawn());
        // 先更新相机，再按“本帧新朝向”换算移动，所以转镜过程中方向连续跟随画面。
        if (CameraPawn && !bGamePaused) { CameraPawn->AdvanceOrbit(DeltaTime); }
        Population->SetSharedInput(bGamePaused || IsMoveInputIgnored() || !CameraPawn
            ? FVector2D::ZeroVector : CameraPawn->ScreenInputToWorld(CurrentScreenInput));
    }
    // 持续按住的 Enhanced Action 必须每帧重新写值；窗口失焦不会留下陈旧移动输入。
    CurrentScreenInput = FVector2D::ZeroVector;
}

void ACrowdPlayerController::Restart()
{
    CurrentScreenInput = FVector2D::ZeroVector;
    // Director 会验证“恰好一个”的约束；这里只取第一个发起重置。
    for (TActorIterator<ACrowdLevelDirector> It(GetWorld()); It; ++It)
    {
        It->ResetCrowd();
        break;
    }
}

void ACrowdPlayerController::RotateLeft()
{
    if (ACrowdCameraPawn* CameraPawn = Cast<ACrowdCameraPawn>(GetPawn()))
    {
        // 从玩家俯视画面观察，正 Yaw 对应向左环绕场景。
        CameraPawn->RequestQuarterTurn(1);
    }
}

void ACrowdPlayerController::RotateRight()
{
    if (ACrowdCameraPawn* CameraPawn = Cast<ACrowdCameraPawn>(GetPawn()))
    {
        CameraPawn->RequestQuarterTurn(-1);
    }
}

void ACrowdPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Controller 离开时立即刹住人群，避免切图/失去 Player 时继续使用最后输入。
    if (UCrowdPopulationSubsystem* Population = GetWorld()->GetSubsystem<UCrowdPopulationSubsystem>())
    {
        Population->SetSharedInput(FVector2D::ZeroVector);
    }
    if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
    {
        if (UEnhancedInputLocalPlayerSubsystem* Input = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
        {
            // 只移除本类添加过的 Context，不影响其它系统的输入映射。
            Input->RemoveMappingContext(InputMapping);
            if (OrbitMapping) { Input->RemoveMappingContext(OrbitMapping); }
        }
    }
    Super::EndPlay(EndPlayReason);
}
