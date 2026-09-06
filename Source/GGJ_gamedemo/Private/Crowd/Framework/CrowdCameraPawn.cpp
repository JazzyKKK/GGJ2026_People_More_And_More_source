// 斜俯视透视轨道相机、90° 缓动状态机和移动方向投影实现。
// CameraPawn 是观察者 Pawn，不是人群中的物理小人。
#include "Crowd/Framework/CrowdCameraPawn.h"
#include "Camera/CameraComponent.h"

ACrowdCameraPawn::ACrowdCameraPawn()
{
    // 镜头由 Controller::PostProcessInput 显式推进，不使用 Pawn Tick，避免输入/镜头先后顺序不确定。
    PrimaryActorTick.bCanEverTick = false;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    Camera->SetupAttachment(RootComponent);
    Camera->ProjectionMode = ECameraProjectionMode::Perspective;
    Camera->AspectRatio = 16.f / 9.f;
    Camera->bConstrainAspectRatio = true;
    // 使用相机局部固定曝光，不修改整个项目，也不受不同关卡自动曝光适应过程影响。
    Camera->PostProcessSettings.bOverride_AutoExposureMethod = true;
    Camera->PostProcessSettings.AutoExposureMethod = AEM_Manual;
    Camera->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
    Camera->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure = false;
    Camera->PostProcessSettings.bOverride_AutoExposureBias = true;
    Camera->PostProcessSettings.AutoExposureBias = 0.f;
    Camera->PostProcessSettings.bOverride_BloomIntensity = true;
    Camera->PostProcessSettings.BloomIntensity = 0.f;
    Camera->PostProcessSettings.bOverride_MotionBlurAmount = true;
    Camera->PostProcessSettings.MotionBlurAmount = 0.f;
    Camera->PostProcessSettings.bOverride_VignetteIntensity = true;
    Camera->PostProcessSettings.VignetteIntensity = 0.f;
    // GameMode 会自动生成并让 Controller 占有该 Pawn，不允许场景实例自行抢占 Player 0。
    AutoPossessPlayer = EAutoReceiveInput::Disabled;
    ApplyOrbitPose();
}

void ACrowdCameraPawn::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    ResetView();
}

void ACrowdCameraPawn::BeginPlay()
{
    Super::BeginPlay();
    ResetView();
}

void ACrowdCameraPawn::ConfigureView(const FVector& Center)
{
    // Pawn 代表轨道圆心，Camera 组件保持相对圆心的位置。
    SetActorLocation(Center);
    SetActorRotation(FRotator::ZeroRotator);
    ResetView();
}

void ACrowdCameraPawn::ApplySettings(const FCrowdCameraSettings& NewSettings)
{
    // 外部传入值先净化，避免零时长、NaN 等值破坏插值。
    OrbitSettings = NewSettings.Sanitized();
    ResetView();
}

void ACrowdCameraPawn::ResetView()
{
    OrbitSettings = OrbitSettings.Sanitized();
    // 链式赋值保证三个角完全一致，相当于取消未完成的过渡。
    CurrentYaw = StartYaw = TargetYaw = OrbitSettings.InitialYaw;
    Elapsed = 0.f;
    bRotating = false;
    ApplyOrbitPose();
}

bool ACrowdCameraPawn::RequestQuarterTurn(int32 Direction)
{
    if (bRotating || Direction == 0) { return false; }
    StartYaw = CurrentYaw;
    // 插值使用未归一化角度：跨过 +180/-180 时仍沿指定方向精确走 90°，不会反向绕远路。
    TargetYaw = StartYaw + (Direction > 0 ? 90.f : -90.f);
    Elapsed = 0.f;
    bRotating = true;
    OnRotationStarted.Broadcast();
    return true;
}

void ACrowdCameraPawn::AdvanceOrbit(float DeltaTime)
{
    // 忽略暂停、无效和倒退时间；Controller 只在非暂停状态调用，但这里仍做边界保护。
    if (!bRotating || !FMath::IsFinite(DeltaTime) || DeltaTime <= 0.f) { return; }
    Elapsed = FMath::Min(Elapsed + DeltaTime, OrbitSettings.TurnDuration);
    const float Alpha = Elapsed / OrbitSettings.TurnDuration;
    // InterpEaseInOut 在前半段加速、后半段减速，指数来自蓝图配置。
    const float Eased = FMath::InterpEaseInOut(0.f, 1.f, Alpha, OrbitSettings.EaseExponent);
    CurrentYaw = FMath::Lerp(StartYaw, TargetYaw, Eased);
    ApplyOrbitPose();
    if (Alpha >= 1.f)
    {
        // 只在完成时归一化角度，防止连续转动造成浮点角度无限增长。
        CurrentYaw = FMath::UnwindDegrees(TargetYaw);
        bRotating = false;
        ApplyOrbitPose();
        OnRotationFinished.Broadcast();
    }
}

void ACrowdCameraPawn::ApplyOrbitPose()
{
    const FRotator ViewRotation(OrbitSettings.Pitch, CurrentYaw, 0.f);
    // ViewRotation.Vector() 是从相机指向中心的前向量；取反后得到相机相对圆心的位置。
    Camera->SetRelativeLocation(-ViewRotation.Vector() * OrbitSettings.OrbitDistance);
    Camera->SetRelativeRotation(ViewRotation);
    // 每次都强制透视，确保旧蓝图遗留的 Orthographic 默认值不会覆盖当前设计。
    Camera->ProjectionMode = ECameraProjectionMode::Perspective;
    Camera->SetFieldOfView(OrbitSettings.FieldOfView);
}

FVector2D ACrowdCameraPawn::ScreenInputToWorld(FVector2D Input) const
{
    // 只取相机 Right/Up 在地面上的投影，并分别单位化，避免俯角缩短“屏幕向上”的输入强度。
    const FVector Right = Camera->GetRightVector().GetSafeNormal2D();
    const FVector Up = Camera->GetUpVector().GetSafeNormal2D();
    const FVector WorldDirection = Right * Input.X + Up * Input.Y;
    // 最终单位圆限制由 PopulationSubsystem 统一完成，键盘斜向不会比单轴更快。
    return FVector2D(WorldDirection.X, WorldDirection.Y);
}
