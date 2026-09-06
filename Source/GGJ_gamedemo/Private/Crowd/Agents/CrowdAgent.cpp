// 单个同步小人的组件搭建、权威物理初始化和销毁清理实现。
// 物理球决定玩法，VisualRoot 下的网格只决定 3D 外观。
#include "Crowd/Agents/CrowdAgent.h"
#include "Crowd/Agents/CrowdPhysicsMovementComponent.h"
#include "Crowd/Core/CrowdPopulationSubsystem.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "UObject/ConstructorHelpers.h"

// 构造函数只创建“类默认组件”和安全默认值；真正的关卡参数在 InitializePhysics 中应用。
ACrowdAgent::ACrowdAgent()
{
    // Actor 自己不 Tick，持续更新交给专用 Movement 组件，职责更清晰。
    PrimaryActorTick.bCanEverTick = false;

    // 球体是根组件，所以 Chaos 更新球的位置时 Actor 和所有视觉组件都会跟随。
    Body = CreateDefaultSubobject<USphereComponent>(TEXT("Body"));
    SetRootComponent(Body);
    Body->InitSphereRadius(30.f);
    Body->SetCollisionObjectType(ECC_PhysicsBody);
    Body->SetCollisionResponseToAllChannels(ECR_Block);
    Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Body->SetEnableGravity(false);
    Body->SetGenerateOverlapEvents(false);
    Body->BodyInstance.bLockZTranslation = true;
    Body->BodyInstance.bLockXRotation = true;
    Body->BodyInstance.bLockYRotation = true;
    Body->BodyInstance.bLockZRotation = true;
    Body->BodyInstance.DOFMode = EDOFMode::SixDOF;
    Body->BodyInstance.bUseCCD = true;
    Body->BodyInstance.PositionSolverIterationCount = 8;
    Body->BodyInstance.VelocitySolverIterationCount = 4;

    // 视觉层与物理层分离：换模型、缩放外观都不会改变球形碰撞。
    VisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VisualRoot"));
    VisualRoot->SetupAttachment(Body);
    VisualMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisualMesh"));
    VisualMesh->SetupAttachment(VisualRoot);
    VisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    VisualMesh->SetRelativeScale3D(FVector(0.42f, 0.42f, 0.54f));
    VisualMesh->SetRelativeLocation(FVector(0.f, 0.f, -3.f));
    HeadMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeadMesh"));
    HeadMesh->SetupAttachment(VisualRoot);
    HeadMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    HeadMesh->SetRelativeScale3D(FVector(0.38f));
    HeadMesh->SetRelativeLocation(FVector(0.f, 0.f, 40.f));

    // 原生类使用 Engine 基础网格兜底；BP_CrowdAgent 可继续替换这些网格。
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    VisualMesh->SetStaticMesh(Cylinder.Object);
    HeadMesh->SetStaticMesh(Sphere.Object);
    Movement = CreateDefaultSubobject<UCrowdPhysicsMovementComponent>(TEXT("Movement"));
}

void ACrowdAgent::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    // 让编辑器视口中立即看到蓝图 Class Defaults 的材质效果。
    ApplyVisualOverrides();
}

void ACrowdAgent::ApplyVisualOverrides()
{
    // 空引用代表“保留模型自身材质”，便于换成自带材质的完整人物模型。
    if (BodyMaterial) { VisualMesh->SetMaterial(0, BodyMaterial); }
    if (HeadMaterial) { HeadMesh->SetMaterial(0, HeadMaterial); }
}

void ACrowdAgent::InitializePhysics(const FCrowdPhysicsSettings& Settings)
{
    ApplyVisualOverrides();

    // Actor/Body 缩放会让物理半径含义变得不明确，因此玩法体始终保持 1:1。
    SetActorScale3D(FVector::OneVector);
    Body->SetSphereRadius(Settings.Radius);
    Body->SetCollisionObjectType(ECC_PhysicsBody);
    Body->SetCollisionResponseToAllChannels(ECR_Block);
    Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Body->SetEnableGravity(false);
    Body->SetLinearDamping(Settings.LinearDamping);
    Body->SetAngularDamping(10.f);
    Body->BodyInstance.bLockZTranslation = true;
    Body->BodyInstance.bLockXRotation = true;
    Body->BodyInstance.bLockYRotation = true;
    Body->BodyInstance.bLockZRotation = true;

    // 低摩擦、零弹性更适合连续推挤；Min 合并可避免场景高弹性材质让人群乱跳。
    RuntimeMaterial = NewObject<UPhysicalMaterial>(this);
    RuntimeMaterial->Friction = 0.2f;
    RuntimeMaterial->Restitution = 0.f;
    RuntimeMaterial->bOverrideRestitutionCombineMode = true;
    RuntimeMaterial->RestitutionCombineMode = EFrictionCombineMode::Min;
    Body->SetPhysMaterialOverride(RuntimeMaterial);

    // 先完成碰撞/约束配置，再开启模拟，避免初始化中的一帧使用错误设置。
    Body->SetSimulatePhysics(true);
    Body->SetMassOverrideInKg(NAME_None, Settings.MassKg, true);
    Body->SetConstraintMode(EDOFMode::SixDOF);
    Movement->Initialize(Body, Settings);
}

void ACrowdAgent::ActivateMovement()
{
    // PopulationSubsystem 只有在初始化和注册都成功后才调用这里。
    Movement->SetComponentTickEnabled(true);
}

void ACrowdAgent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // 先停止更新，防止 World 销毁阶段继续访问 Subsystem 或物理体。
    Movement->SetComponentTickEnabled(false);
    if (UCrowdPopulationSubsystem* Population = GetWorld()->GetSubsystem<UCrowdPopulationSubsystem>())
    {
        Population->UnregisterAgent(this);
    }
    Super::EndPlay(EndPlayReason);
}
