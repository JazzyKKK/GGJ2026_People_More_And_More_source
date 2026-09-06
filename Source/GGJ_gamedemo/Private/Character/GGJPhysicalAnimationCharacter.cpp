// 官方第三人称 CharacterMovement 与主动布娃娃的组合实现。
#include "Character/GGJPhysicalAnimationCharacter.h"

#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/AnimationAsset.h"
#include "Character/GGJCharacterGroupManager.h"
#include "Character/GGJGroupCameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "TimerManager.h"

AGGJPhysicalAnimationCharacter::AGGJPhysicalAnimationCharacter()
{
    PrimaryActorTick.bCanEverTick = true;

    // 与 UE 5.6 Third Person C++ 模板保持一致，蓝图中仍可覆盖这些移动参数。
    GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);
    bUseControllerRotationPitch = false;
    bUseControllerRotationYaw = false;
    bUseControllerRotationRoll = false;

    UCharacterMovementComponent* Movement = GetCharacterMovement();
    Movement->bOrientRotationToMovement = true;
    // 群体成员不各自创建 AIController；允许 CharacterMovement 在无 Controller 时继续消费共享输入。
    Movement->bRunPhysicsWithNoController = true;
    Movement->RotationRate = FRotator(0.f, 500.f, 0.f);
    Movement->JumpZVelocity = 500.f;
    Movement->AirControl = 0.35f;
    Movement->MaxWalkSpeed = 500.f;
    Movement->MinAnalogWalkSpeed = 20.f;
    Movement->BrakingDecelerationWalking = 2000.f;
    Movement->BrakingDecelerationFalling = 1500.f;

    // Mesh 的默认相对变换适配 Mixamo 常见的 Z 向上、面向 +Y 导入结果。
    // 如果下一版模型轴向或脚底高度不同，只需在 BP 的 Mesh 组件中覆盖。
    GetMesh()->SetRelativeLocation(FVector(0.f, 0.f, -96.f));
    GetMesh()->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
    GetMesh()->SetCollisionProfileName(TEXT("Pawn"));
    GetMesh()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    GetMesh()->SetGenerateOverlapEvents(false);

    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(RootComponent);
    CameraBoom->TargetArmLength = 400.f;
    CameraBoom->bUsePawnControlRotation = true;
    CameraBoom->SocketOffset = FVector(0.f, 0.f, 45.f);

    FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
    FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
    FollowCamera->bUsePawnControlRotation = false;

    PhysicalAnimation = CreateDefaultSubobject<UPhysicalAnimationComponent>(TEXT("PhysicalAnimation"));
    PhysicalAnimation->StrengthMultiplyer = MuscleStrength;

    // 精确复刻 Test_Lab2 / BP_ThirdPersonCharacter 中 Make Physical Animation Data 的默认值。
    DriveSettings.bIsLocalSimulation = true;
    DriveSettings.OrientationStrength = 100.f;
    DriveSettings.AngularVelocityStrength = 100.f;
    DriveSettings.PositionStrength = 100.f;
    DriveSettings.VelocityStrength = 100.f;
    DriveSettings.MaxLinearForce = 0.f;
    DriveSettings.MaxAngularForce = 0.f;
}

void AGGJPhysicalAnimationCharacter::BeginPlay()
{
    Super::BeginPlay();
    EnsureInputAssets();
    AddInputMapping();

    // Single Node 模式不要求额外 AnimBlueprint，Game Jam 阶段更容易替换同骨架动画。
    GetMesh()->SetAnimationMode(EAnimationMode::AnimationSingleNode);
    UpdateLocomotionAnimation();

    if (bEnablePhysicalAnimationOnBeginPlay)
    {
        // FinishSpawning 会立刻触发 BeginPlay，但 SkeletalMesh 的 Physics State 可能要到下一帧
        // 才拥有完整 Bodies。UE 5.6 的 PhysicalAnimation 内部会直接 Bodies[BodyIndex]，
        // 因此动态生成多人时必须等组件真正准备好。
        SchedulePhysicalAnimationInitialization();
    }
}

void AGGJPhysicalAnimationCharacter::Tick(const float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    UpdateLocomotionAnimation();
}

bool AGGJPhysicalAnimationCharacter::InitializePhysicalAnimation()
{
    bPhysicalAnimationReady = false;
    USkeletalMeshComponent* MeshComponent = GetMesh();
    if (!MeshComponent || !MeshComponent->GetSkeletalMeshAsset())
    {
        UE_LOG(LogTemp, Error, TEXT("%s: Mesh 没有 Skeletal Mesh，无法启用 Physical Animation。"), *GetName());
        return false;
    }
    if (!MeshComponent->GetPhysicsAsset())
    {
        UE_LOG(LogTemp, Error, TEXT("%s: %s 没有 PhysicsAsset。请在蓝图 Mesh 或 Skeletal Mesh 中指定。"),
            *GetName(), *GetNameSafe(MeshComponent->GetSkeletalMeshAsset()));
        return false;
    }
    if (DrivenBoneName.IsNone() || MeshComponent->GetBoneIndex(DrivenBoneName) == INDEX_NONE)
    {
        UE_LOG(LogTemp, Error, TEXT("%s: 起始骨骼 '%s' 不存在于 %s。"),
            *GetName(), *DrivenBoneName.ToString(), *GetNameSafe(MeshComponent->GetSkeletalMeshAsset()));
        return false;
    }
    if (!IsPhysicalBodyStateReady())
    {
        UE_LOG(LogTemp, Verbose, TEXT("%s: SkeletalMesh Physics State 尚未准备完成，暂缓 Physical Animation。"),
            *GetName());
        return false;
    }

    // 顺序与 Test_Lab2 的 phyAni Event BeginPlay 完全一致。
    PhysicalAnimation->SetSkeletalMeshComponent(MeshComponent);
    PhysicalAnimation->ApplyPhysicalAnimationSettingsBelow(DrivenBoneName, DriveSettings, true);
    MeshComponent->SetAllBodiesBelowSimulatePhysics(DrivenBoneName, true, bIncludeDrivenBoneInSimulation);
    PhysicalAnimation->SetStrengthMultiplyer(MuscleStrength);

    // 蓝图原型使用 Pawn Profile + QueryAndPhysics；这里显式恢复，防止换模型时被资产默认值覆盖。
    MeshComponent->SetCollisionProfileName(TEXT("Pawn"));
    MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    MeshComponent->WakeAllRigidBodies();

    int32 SimulatingBodies = 0;
    for (const USkeletalBodySetup* BodySetup : MeshComponent->GetPhysicsAsset()->SkeletalBodySetups)
    {
        if (BodySetup)
        {
            if (const FBodyInstance* Body = MeshComponent->GetBodyInstance(BodySetup->BoneName))
            {
                SimulatingBodies += Body->IsInstanceSimulatingPhysics() ? 1 : 0;
            }
        }
    }
    if (SimulatingBodies == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("%s: PhysicsAsset 在 '%s' 下没有可模拟的子形体。"),
            *GetName(), *DrivenBoneName.ToString());
        return false;
    }

    bPhysicalAnimationReady = true;
    UE_LOG(LogTemp, Display, TEXT("%s: Physical Animation 已启用，%d 个子形体正在模拟。"),
        *GetName(), SimulatingBodies);
    return true;
}

void AGGJPhysicalAnimationCharacter::SchedulePhysicalAnimationInitialization()
{
    if (bEndingPlay || !GetWorld() || !bEnablePhysicalAnimationOnBeginPlay
        || bPhysicalAnimationReady)
    {
        return;
    }

    const float Delay = FMath::Max(PhysicalAnimationInitializationDelay, UE_KINDA_SMALL_NUMBER);
    GetWorldTimerManager().SetTimer(PhysicalAnimationInitializationTimer, this,
        &AGGJPhysicalAnimationCharacter::TryInitializePhysicalAnimationDeferred, Delay, false);
}

void AGGJPhysicalAnimationCharacter::TryInitializePhysicalAnimationDeferred()
{
    if (bEndingPlay || bPhysicalAnimationReady) { return; }

    ++PhysicalAnimationInitializationAttempts;
    if (IsPhysicalBodyStateReady() && InitializePhysicalAnimation())
    {
        PhysicalAnimationInitializationAttempts = 0;
        return;
    }

    // 动态 Spawn、切换关卡或资产异步初始化时继续等待。限制次数可避免坏资产永久计时。
    if (PhysicalAnimationInitializationAttempts < 120)
    {
        SchedulePhysicalAnimationInitialization();
    }
    else
    {
        UE_LOG(LogTemp, Error,
            TEXT("%s: 等待 SkeletalMesh Physics State 超时，Physical Animation 未启用。请检查 PhysicsAsset。"),
            *GetName());
    }
}

bool AGGJPhysicalAnimationCharacter::IsPhysicalBodyStateReady() const
{
    const USkeletalMeshComponent* MeshComponent = GetMesh();
    const UPhysicsAsset* PhysicsAsset = MeshComponent ? MeshComponent->GetPhysicsAsset() : nullptr;
    if (!MeshComponent || !PhysicsAsset || !MeshComponent->IsPhysicsStateCreated()
        || MeshComponent->Bodies.IsEmpty())
    {
        return false;
    }

    // PhysicalAnimationComponent.cpp 内部会按 PhysicsAsset BodyIndex 直接访问 Bodies；
    // 在调用它之前逐项验证，绕开 UE 5.6 缺少边界检查导致的 Array[0]/size 0 崩溃。
    for (int32 BodyIndex = 0; BodyIndex < PhysicsAsset->SkeletalBodySetups.Num(); ++BodyIndex)
    {
        if (!MeshComponent->Bodies.IsValidIndex(BodyIndex) || !MeshComponent->Bodies[BodyIndex])
        {
            return false;
        }
    }
    return true;
}

void AGGJPhysicalAnimationCharacter::SetMuscleStrength(const float NewStrength)
{
    MuscleStrength = FMath::Max(0.f, NewStrength);
    PhysicalAnimation->SetStrengthMultiplyer(MuscleStrength);
}

void AGGJPhysicalAnimationCharacter::ResetPhysicalAnimation()
{
    USkeletalMeshComponent* MeshComponent = GetMesh();
    if (!MeshComponent) { return; }

    // 先变回运动学身体以清掉碰撞累积，再按同一配置重新建立马达。
    MeshComponent->SetAllBodiesBelowSimulatePhysics(DrivenBoneName, false, bIncludeDrivenBoneInSimulation);
    MeshComponent->SetPhysicsLinearVelocity(FVector::ZeroVector, false);
    MeshComponent->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector, false);
    if (!InitializePhysicalAnimation())
    {
        SchedulePhysicalAnimationInitialization();
    }
}

void AGGJPhysicalAnimationCharacter::AddBoneImpulse(const FName BoneName, const FVector Impulse,
    const bool bVelocityChange)
{
    if (bPhysicalAnimationReady && !BoneName.IsNone())
    {
        GetMesh()->AddImpulse(Impulse, BoneName, bVelocityChange);
    }
}

void AGGJPhysicalAnimationCharacter::EnsureInputAssets()
{
    // Q/E 独立成一套 Context：即使蓝图使用官方第三人称 IMC，也一定拥有群体转镜输入。
    if (!OrbitInputMapping || !RotateLeftAction || !RotateRightAction)
    {
        OrbitInputMapping = NewObject<UInputMappingContext>(this, TEXT("IMC_GroupCamera_Runtime"));
        RotateLeftAction = NewObject<UInputAction>(this, TEXT("IA_GroupCameraLeft_Runtime"));
        RotateRightAction = NewObject<UInputAction>(this, TEXT("IA_GroupCameraRight_Runtime"));
        RotateLeftAction->ValueType = EInputActionValueType::Boolean;
        RotateRightAction->ValueType = EInputActionValueType::Boolean;
        OrbitInputMapping->MapKey(RotateLeftAction, EKeys::Q);
        OrbitInputMapping->MapKey(RotateRightAction, EKeys::E);
    }

    // 三个 Action 与 Context 必须属于同一套；任何一个缺失就生成完整、可直接玩的兜底映射。
    if (InputMapping && MoveAction && LookAction && JumpAction) { return; }

    InputMapping = NewObject<UInputMappingContext>(this, TEXT("IMC_PhysicalCharacter_Runtime"));
    MoveAction = NewObject<UInputAction>(this, TEXT("IA_Move_Runtime"));
    LookAction = NewObject<UInputAction>(this, TEXT("IA_Look_Runtime"));
    JumpAction = NewObject<UInputAction>(this, TEXT("IA_Jump_Runtime"));
    MoveAction->ValueType = EInputActionValueType::Axis2D;
    MoveAction->AccumulationBehavior = EInputActionAccumulationBehavior::Cumulative;
    LookAction->ValueType = EInputActionValueType::Axis2D;
    LookAction->AccumulationBehavior = EInputActionAccumulationBehavior::Cumulative;
    JumpAction->ValueType = EInputActionValueType::Boolean;

    // 键盘单键先产生 X，Swizzle 把前后键转换到 Y；Negate 生成反方向。
    auto MapMove = [this](const FKey Key, const bool bNegative, const bool bY)
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
    MapMove(EKeys::D, false, false);
    MapMove(EKeys::A, true, false);
    MapMove(EKeys::W, false, true);
    MapMove(EKeys::S, true, true);
    MapMove(EKeys::Right, false, false);
    MapMove(EKeys::Left, true, false);
    MapMove(EKeys::Up, false, true);
    MapMove(EKeys::Down, true, true);

    FEnhancedActionKeyMapping& MoveStick = InputMapping->MapKey(MoveAction, EKeys::Gamepad_Left2D);
    MoveStick.Modifiers.Add(NewObject<UInputModifierDeadZone>(InputMapping));

    // 鼠标向上应抬高镜头，所以只反转 Mouse2D 的 Y；手柄右摇杆保持原始方向。
    FEnhancedActionKeyMapping& MouseLook = InputMapping->MapKey(LookAction, EKeys::Mouse2D);
    UInputModifierNegate* InvertMouseY = NewObject<UInputModifierNegate>(InputMapping);
    InvertMouseY->bX = false;
    InvertMouseY->bY = true;
    InvertMouseY->bZ = false;
    MouseLook.Modifiers.Add(InvertMouseY);
    FEnhancedActionKeyMapping& LookStick = InputMapping->MapKey(LookAction, EKeys::Gamepad_Right2D);
    LookStick.Modifiers.Add(NewObject<UInputModifierDeadZone>(InputMapping));

    InputMapping->MapKey(JumpAction, EKeys::SpaceBar);
    InputMapping->MapKey(JumpAction, EKeys::Gamepad_FaceButton_Bottom);
}

void AGGJPhysicalAnimationCharacter::AddInputMapping()
{
    if (const APlayerController* PlayerController = Cast<APlayerController>(GetController()))
    {
        if (ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer())
        {
            if (UEnhancedInputLocalPlayerSubsystem* Input =
                LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
            {
                if (!bInputMappingAdded && InputMapping)
                {
                    Input->AddMappingContext(InputMapping, 0);
                    bInputMappingAdded = true;
                }
                if (!bOrbitInputMappingAdded && OrbitInputMapping)
                {
                    Input->AddMappingContext(OrbitInputMapping, 1);
                    bOrbitInputMappingAdded = true;
                }
            }
        }
    }
}

void AGGJPhysicalAnimationCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
    EnsureInputAssets();
    // 默认 Pawn 可能先 BeginPlay、随后才被 PlayerController Possess；此时在输入组件建立阶段补加 Context。
    AddInputMapping();
    if (UEnhancedInputComponent* Enhanced = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
        Enhanced->BindAction(MoveAction, ETriggerEvent::Triggered, this,
            &AGGJPhysicalAnimationCharacter::HandleMove);
        Enhanced->BindAction(LookAction, ETriggerEvent::Triggered, this,
            &AGGJPhysicalAnimationCharacter::HandleLook);
        Enhanced->BindAction(JumpAction, ETriggerEvent::Started, this,
            &AGGJPhysicalAnimationCharacter::DoJumpStart);
        Enhanced->BindAction(JumpAction, ETriggerEvent::Completed, this,
            &AGGJPhysicalAnimationCharacter::DoJumpEnd);
        Enhanced->BindAction(RotateLeftAction, ETriggerEvent::Started, this,
            &AGGJPhysicalAnimationCharacter::RotateGroupCameraLeft);
        Enhanced->BindAction(RotateRightAction, ETriggerEvent::Started, this,
            &AGGJPhysicalAnimationCharacter::RotateGroupCameraRight);
    }
}

void AGGJPhysicalAnimationCharacter::HandleMove(const FInputActionValue& Value)
{
    const FVector2D Axis = Value.Get<FVector2D>();
    DoMove(Axis.X, Axis.Y);
}

void AGGJPhysicalAnimationCharacter::HandleLook(const FInputActionValue& Value)
{
    const FVector2D Axis = Value.Get<FVector2D>();
    DoLook(Axis.X, Axis.Y);
}

void AGGJPhysicalAnimationCharacter::DoMove(const float Right, const float Forward)
{
    if (!Controller) { return; }
    FVector WorldMovement;
    AGGJCharacterGroupManager* Manager = CharacterGroupManager.Get();
    if (Manager && Manager->GetGroupCamera())
    {
        // 转镜过程中也实时跟随画面方向，W 始终代表屏幕上方。
        WorldMovement = Manager->GetGroupCamera()->ScreenInputToWorld(FVector2D(Right, Forward));
    }
    else
    {
        const FRotator YawRotation(0.f, Controller->GetControlRotation().Yaw, 0.f);
        WorldMovement = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X) * Forward
            + FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y) * Right;
    }
    WorldMovement = WorldMovement.GetClampedToMaxSize(1.f);

    // 登记为 Leader 后只发布一次；Manager 再把完全相同的世界方向发给每个成员。
    if (Manager)
    {
        Manager->ApplySharedMovement(WorldMovement);
    }
    else
    {
        ApplySharedWorldMovement(WorldMovement);
    }
}

void AGGJPhysicalAnimationCharacter::ApplySharedWorldMovement(FVector WorldMovement)
{
    if (WorldMovement.ContainsNaN()) { return; }
    WorldMovement.Z = 0.f;
    const float Strength = FMath::Min(WorldMovement.Size(), 1.f);
    if (Strength > UE_KINDA_SMALL_NUMBER)
    {
        // bForce=true：没有 Controller 的另外两名 Character 也会接受这一帧输入。
        AddMovementInput(WorldMovement / Strength, Strength, true);
    }
}

void AGGJPhysicalAnimationCharacter::SetCharacterGroupManager(
    AGGJCharacterGroupManager* NewManager)
{
    CharacterGroupManager = NewManager;
}

void AGGJPhysicalAnimationCharacter::DoLook(const float Yaw, const float Pitch)
{
    AddControllerYawInput(Yaw);
    AddControllerPitchInput(Pitch);
}

void AGGJPhysicalAnimationCharacter::DoJumpStart()
{
    if (AGGJCharacterGroupManager* Manager = CharacterGroupManager.Get())
    {
        Manager->ApplySharedJumpStart();
    }
    else
    {
        ApplySharedJumpStart();
    }
}

void AGGJPhysicalAnimationCharacter::DoJumpEnd()
{
    if (AGGJCharacterGroupManager* Manager = CharacterGroupManager.Get())
    {
        Manager->ApplySharedJumpEnd();
    }
    else
    {
        ApplySharedJumpEnd();
    }
}

void AGGJPhysicalAnimationCharacter::ApplySharedJumpStart()
{
    Jump();
}

void AGGJPhysicalAnimationCharacter::ApplySharedJumpEnd()
{
    StopJumping();
}

void AGGJPhysicalAnimationCharacter::RotateGroupCameraLeft()
{
    if (AGGJCharacterGroupManager* Manager = CharacterGroupManager.Get())
    {
        if (AGGJGroupCameraActor* GroupCamera = Manager->GetGroupCamera())
        {
            // 从玩家俯视画面观察，正 Yaw 对应向左环绕场景。
            GroupCamera->RequestQuarterTurn(1);
        }
    }
}

void AGGJPhysicalAnimationCharacter::RotateGroupCameraRight()
{
    if (AGGJCharacterGroupManager* Manager = CharacterGroupManager.Get())
    {
        if (AGGJGroupCameraActor* GroupCamera = Manager->GetGroupCamera())
        {
            GroupCamera->RequestQuarterTurn(-1);
        }
    }
}

void AGGJPhysicalAnimationCharacter::UpdateLocomotionAnimation()
{
    const float Speed = GetVelocity().Size2D();
    EGGJLocomotionState DesiredState = EGGJLocomotionState::Walk;
    UAnimationAsset* DesiredAnimation = WalkAnimation;
    if (Speed <= IdleSpeedThreshold)
    {
        DesiredState = EGGJLocomotionState::Idle;
        DesiredAnimation = IdleAnimation;
    }
    else if (Speed >= RunSpeedThreshold)
    {
        DesiredState = EGGJLocomotionState::Run;
        DesiredAnimation = RunAnimation ? RunAnimation.Get() : WalkAnimation.Get();
    }

    // 错误 Skeleton 的 AnimSequence 直接交给 Single Node 可能访问越界。
    // 保留当前兼容姿势并只警告一次，让关卡仍可运行，方便美术随后做 Retarget。
    if (DesiredAnimation && !IsAnimationCompatible(DesiredAnimation))
    {
        if (LastRejectedAnimation != DesiredAnimation)
        {
            UE_LOG(LogTemp, Warning,
                TEXT("%s: 动画 %s 的 Skeleton 与角色 Mesh 不兼容，已保持兼容的 Idle 姿势。"),
                *GetName(), *GetNameSafe(DesiredAnimation));
            LastRejectedAnimation = DesiredAnimation;
        }
        if (IdleAnimation && IsAnimationCompatible(IdleAnimation))
        {
            DesiredState = EGGJLocomotionState::Idle;
            DesiredAnimation = IdleAnimation;
        }
        else
        {
            DesiredAnimation = nullptr;
        }
    }
    else if (DesiredAnimation)
    {
        LastRejectedAnimation = nullptr;
    }

    if (DesiredAnimation && (DesiredState != LocomotionState || DesiredAnimation != CurrentAnimation))
    {
        PlayLocomotionAnimation(DesiredState, DesiredAnimation);
    }

    if (UAnimSingleNodeInstance* SingleNode = GetMesh()->GetSingleNodeInstance())
    {
        float Rate = 1.f;
        if (DesiredState == EGGJLocomotionState::Walk)
        {
            Rate = FMath::Clamp(Speed / 170.f, 0.65f, 1.45f);
        }
        else if (DesiredState == EGGJLocomotionState::Run)
        {
            Rate = FMath::Clamp(Speed / 430.f, 0.75f, 1.35f);
        }
        SingleNode->SetPlayRate(Rate);
    }
}

bool AGGJPhysicalAnimationCharacter::IsAnimationCompatible(const UAnimationAsset* Animation) const
{
    const USkeletalMeshComponent* MeshComponent = GetMesh();
    const USkeletalMesh* SkeletalMesh = MeshComponent ? MeshComponent->GetSkeletalMeshAsset() : nullptr;
    return Animation && SkeletalMesh && Animation->GetSkeleton()
        && Animation->GetSkeleton() == SkeletalMesh->GetSkeleton();
}

void AGGJPhysicalAnimationCharacter::PlayLocomotionAnimation(const EGGJLocomotionState NewState,
    UAnimationAsset* Animation)
{
    if (!Animation || !IsAnimationCompatible(Animation)) { return; }
    GetMesh()->PlayAnimation(Animation, true);
    LocomotionState = NewState;
    CurrentAnimation = Animation;
}

void AGGJPhysicalAnimationCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    bEndingPlay = true;
    GetWorldTimerManager().ClearTimer(PhysicalAnimationInitializationTimer);
    if (AGGJCharacterGroupManager* Manager = CharacterGroupManager.Get())
    {
        CharacterGroupManager.Reset();
        Manager->UnregisterMember(this);
    }
    if (bInputMappingAdded)
    {
        if (const APlayerController* PlayerController = Cast<APlayerController>(GetController()))
        {
            if (ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer())
            {
                if (UEnhancedInputLocalPlayerSubsystem* Input =
                    LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
                {
                    Input->RemoveMappingContext(InputMapping);
                }
            }
        }
        bInputMappingAdded = false;
    }
    if (bOrbitInputMappingAdded)
    {
        if (const APlayerController* PlayerController = Cast<APlayerController>(GetController()))
        {
            if (ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer())
            {
                if (UEnhancedInputLocalPlayerSubsystem* Input =
                    LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
                {
                    Input->RemoveMappingContext(OrbitInputMapping);
                }
            }
        }
        bOrbitInputMappingAdded = false;
    }
    Super::EndPlay(EndPlayReason);
}
