// Crowd 原生自动化测试。
// 这些测试只在 WITH_DEV_AUTOMATION_TESTS 的编辑器/开发构建中编译，不进入 Shipping 包体。
// 可在 Session Frontend 中运行 Crowd，也可用 UnrealEditor-Cmd 批量运行。
#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Crowd/Agents/CrowdAgent.h"
#include "Crowd/Agents/CrowdPhysicsMovementComponent.h"
#include "Crowd/Core/CrowdPopulationSubsystem.h"
#include "Crowd/Framework/CrowdLevelDirector.h"
#include "Crowd/Framework/CrowdCameraPawn.h"
#include "Crowd/Framework/CrowdPlayerController.h"
#include "EnhancedPlayerInput.h"
#include "EnhancedInputComponent.h"
#include "Components/SphereComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Camera/CameraComponent.h"

namespace CrowdTests
{
    /**
     * 每个测试独占的最小瞬态 Game World。
     * 它开启 Chaos 与碰撞，但关闭音频、导航、AI、特效和事务，既接近真实运行顺序又保持测试轻量。
     */
    struct FTestWorld
    {
        /** 测试拥有的瞬态 World；构造时创建，析构时彻底销毁。 */
        UWorld* World;

        FTestWorld()
        {
            // 显式开启物理场景/碰撞查询，其余与当前玩法无关的系统全部关闭。
            UWorld::InitializationValues Values;
            Values.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false)
                .CreatePhysicsScene(true).ShouldSimulatePhysics(true).EnableTraceCollision(true)
                .SetTransactional(false).CreateFXSystem(false);
            World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
                ERHIFeatureLevel::Num, &Values);
            GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
            // 按真实游戏顺序初始化 Actor、BeginPlay 和 Match，确保 Subsystem/Director 行为可信。
            World->InitializeActorsForPlay(FURL());
            World->GetWorldSettings()->NotifyBeginPlay();
            World->GetWorldSettings()->NotifyMatchStarted();
            World->BeginPlay();
        }
        ~FTestWorld()
        {
            // 先清人口再结束 World，避免弱引用和物理 Actor 泄漏到下一项测试。
            World->GetSubsystem<UCrowdPopulationSubsystem>()->ClearPopulation();
            World->EndPlay(EEndPlayReason::Quit);
            GEngine->DestroyWorldContext(World);
            World->DestroyWorld(false);
        }
        void Step(int32 Frames, float DeltaTime = 1.f / 60.f)
        {
            // 主动推进固定帧数，测试不依赖编辑器实际渲染帧率。
            for (int32 Index = 0; Index < Frames; ++Index)
            {
                World->Tick(LEVELTICK_All, DeltaTime);
                ++GFrameCounter;
            }
        }

        /** 获取此测试 World 自己的人口子系统。 */
        UCrowdPopulationSubsystem* Population() const { return World->GetSubsystem<UCrowdPopulationSubsystem>(); }

        /** 用默认参数在指定球心生成一个原生小人。 */
        ACrowdAgent* Spawn(const FVector& Position) const
        {
            return Population()->SpawnAgent(ACrowdAgent::StaticClass(), Position, FCrowdPhysicsSettings());
        }

        /** 创建一个只有 Box 碰撞的静态墙，用来验证阻挡和挤压。 */
        void Wall(const FVector& Position, const FVector& Extent)
        {
            AActor* Actor = World->SpawnActor<AActor>();
            UBoxComponent* Box = NewObject<UBoxComponent>(Actor);
            Actor->SetRootComponent(Box);
            Box->SetBoxExtent(Extent);
            Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
            Box->SetCollisionObjectType(ECC_WorldStatic);
            Box->SetCollisionResponseToAllChannels(ECR_Block);
            Box->SetWorldLocation(Position);
            Box->RegisterComponent();
        }
    };
}

// 基础人口测试：输入限制、模拟量保留、出生防重叠、注册/注销和清空状态。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdPopulationTest, "Crowd.M0.PopulationAndInput",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCrowdPopulationTest::RunTest(const FString& Parameters)
{
    CrowdTests::FTestWorld TestWorld;
    UCrowdPopulationSubsystem* Population = TestWorld.Population();
    if (!TestNotNull(TEXT("Game world has population subsystem"), Population)) { return false; }
    Population->SetSharedInput(FVector2D(1, 1));
    TestTrue(TEXT("Diagonal input clamped to unit circle"), FMath::IsNearlyEqual(Population->GetSharedInput().Size(), 1.0));
    Population->SetSharedInput(FVector2D(0.2, 0));
    TestTrue(TEXT("Analog magnitude preserved"), Population->GetSharedInput().Equals(FVector2D(0.2, 0)));
    ACrowdAgent* Agent = TestWorld.Spawn(FVector(0, 0, 33));
    if (!TestNotNull(TEXT("Spawn succeeds"), Agent)) { return false; }
    TestEqual(TEXT("One registered agent"), Population->GetPopulationCount(), 1);
    TestNull(TEXT("Overlapping spawn rejected"), TestWorld.Spawn(FVector(0, 0, 33)));
    TestEqual(TEXT("Rejected spawn never increments count"), Population->GetPopulationCount(), 1);
    Agent->Destroy();
    TestEqual(TEXT("EndPlay unregisters"), Population->GetPopulationCount(), 0);
    Population->ClearPopulation();
    TestTrue(TEXT("Reset clears stale input"), Population->GetSharedInput().IsNearlyZero());
    return true;
}

// 共享移动测试：两个刚体接收同一方向、软限速、松键阻尼和镜头坐标转换。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdMovementTest, "Crowd.M0.SharedMovementAndBraking",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCrowdMovementTest::RunTest(const FString& Parameters)
{
    CrowdTests::FTestWorld TestWorld;
    ACrowdAgent* A = TestWorld.Spawn(FVector(0, -100, 33));
    ACrowdAgent* B = TestWorld.Spawn(FVector(0, 100, 33));
    if (!TestNotNull(TEXT("Agent A"), A) || !TestNotNull(TEXT("Agent B"), B)) { return false; }
    TestWorld.Population()->SetSharedInput(FVector2D(1, 0));
    TestWorld.Step(120);
    TestTrue(TEXT("Shared input moves A"), A->GetActorLocation().X > 300.f);
    TestTrue(TEXT("Shared input moves B equally"), FMath::Abs(A->GetActorLocation().X - B->GetActorLocation().X) < 2.f);
    TestTrue(TEXT("Normal drive speed bounded"), A->Body->GetPhysicsLinearVelocity().Size2D() <= 330.f);
    TestWorld.Population()->SetSharedInput(FVector2D::ZeroVector);
    TestWorld.Step(120);
    TestTrue(TEXT("Release brakes to rest"), A->Body->GetPhysicsLinearVelocity().Size2D() < 1.f);
    ACrowdCameraPawn* Camera = TestWorld.World->SpawnActor<ACrowdCameraPawn>();
    FCrowdCameraSettings CardinalSettings;
    CardinalSettings.InitialYaw = -90.f;
    Camera->ApplySettings(CardinalSettings);
    TestTrue(TEXT("Screen right maps to world +X"), Camera->ScreenInputToWorld(FVector2D(1, 0)).Equals(FVector2D(1, 0), 0.001));
    TestTrue(TEXT("Screen up maps to world -Y"), Camera->ScreenInputToWorld(FVector2D(0, 1)).Equals(FVector2D(0, -1), 0.001));
    return true;
}

// 物理约束测试：小人可互推、墙能阻挡，长时间挤压后仍锁定 Z 与旋转。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdCollisionTest, "Crowd.M0.CollisionAndPlaneLocks",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCrowdCollisionTest::RunTest(const FString& Parameters)
{
    CrowdTests::FTestWorld TestWorld;
    TestWorld.Wall(FVector(400, 0, 70), FVector(20, 300, 100));
    ACrowdAgent* A = TestWorld.Spawn(FVector(0, 0, 33));
    ACrowdAgent* B = TestWorld.Spawn(FVector(90, 0, 33));
    if (!TestNotNull(TEXT("Agent A"), A) || !TestNotNull(TEXT("Agent B"), B)) { return false; }
    B->Movement->SetComponentTickEnabled(false);
    TestWorld.Population()->SetSharedInput(FVector2D(1, 0));
    TestWorld.Step(120);
    TestTrue(TEXT("Driven agent pushes unpowered body"), B->GetActorLocation().X > 130.f);
    A->Body->AddImpulse(FVector(0, 0, 1000), NAME_None, true);
    A->Body->AddAngularImpulseInRadians(FVector(100, 100, 100), NAME_None, true);
    TestWorld.Step(600);
    TestTrue(TEXT("Wall blocks leading agent"), B->GetActorLocation().X <= 352.f);
    TestTrue(TEXT("Agents do not pass through each other"), B->GetActorLocation().X - A->GetActorLocation().X >= 57.f);
    TestTrue(TEXT("Z stays locked during long squeeze"), FMath::Abs(A->GetActorLocation().Z - 33.f) < 0.5f);
    TestTrue(TEXT("Rotation stays locked under impulse"), A->GetActorQuat().AngularDistance(FQuat::Identity) < 0.01f);
    return true;
}

// Director 测试：默认 12 人居中阵列，R 同等的重置不会叠加人口且会清输入。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdDirectorTest, "Crowd.M0.InitialGridAndReset",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCrowdDirectorTest::RunTest(const FString& Parameters)
{
    CrowdTests::FTestWorld TestWorld;
    ACrowdLevelDirector* Director = TestWorld.World->SpawnActor<ACrowdLevelDirector>();
    TestWorld.Step(2);
    TestTrue(TEXT("Director initialization succeeds"), Director->bInitialized);
    TestEqual(TEXT("Default grid has 12 people"), TestWorld.Population()->GetPopulationCount(), 12);
    TestWorld.Population()->SetSharedInput(FVector2D(1, 1));
    TestWorld.Step(30);
    TestTrue(TEXT("Reset succeeds"), Director->ResetCrowd());
    TestEqual(TEXT("Reset does not duplicate population"), TestWorld.Population()->GetPopulationCount(), 12);
    TestTrue(TEXT("Reset input is zero"), TestWorld.Population()->GetSharedInput().IsNearlyZero());
    return true;
}

// Enhanced Input 测试：Axis2D Action 能进入共享世界输入，完成和暂停都能安全清零。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdEnhancedInputTest, "Crowd.M0.EnhancedInputPipeline",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCrowdEnhancedInputTest::RunTest(const FString& Parameters)
{
    CrowdTests::FTestWorld TestWorld;
    ACrowdPlayerController* Controller = TestWorld.World->SpawnActor<ACrowdPlayerController>();
    ACrowdCameraPawn* Camera = TestWorld.World->SpawnActor<ACrowdCameraPawn>();
    FCrowdCameraSettings CardinalSettings;
    CardinalSettings.InitialYaw = -90.f;
    Camera->ApplySettings(CardinalSettings);
    Controller->Possess(Camera);
    UEnhancedPlayerInput* Input = NewObject<UEnhancedPlayerInput>(Controller);
    Controller->PlayerInput = Input;
    Controller->InputComponent = NewObject<UEnhancedInputComponent>(Controller);
    Controller->SetupInputComponent();
    TArray<UInputComponent*> Stack { Controller->InputComponent };
    Input->InjectInputForAction(Controller->MoveAction, FInputActionValue(FVector2D(1, 1)));
    Input->ProcessInputStack(Stack, 1.f / 60.f, false);
    TestTrue(TEXT("Enhanced action reaches shared world input"),
        TestWorld.Population()->GetSharedInput().Equals(FVector2D(1, -1).GetSafeNormal(), 0.001));
    Input->ProcessInputStack(Stack, 1.f / 60.f, false);
    TestTrue(TEXT("Completed action clears input"), TestWorld.Population()->GetSharedInput().IsNearlyZero());
    Input->InjectInputForAction(Controller->MoveAction, FInputActionValue(FVector2D(1, 0)));
    Input->ProcessInputStack(Stack, 1.f / 60.f, true);
    TestTrue(TEXT("Paused input cannot drive crowd"), TestWorld.Population()->GetSharedInput().IsNearlyZero());
    return true;
}

// 帧率一致性测试：同样两秒输入在 30/60/120 FPS 下的自由移动距离误差小于 5%。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdFrameRateTest, "Crowd.M0.FrameRateConsistency",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCrowdFrameRateTest::RunTest(const FString& Parameters)
{
    double ReferenceDistance = 0;
    for (const int32 FrameRate : {30, 60, 120})
    {
        CrowdTests::FTestWorld TestWorld;
        ACrowdAgent* Agent = TestWorld.Spawn(FVector(0, 0, 33));
        if (!TestNotNull(TEXT("Frame-rate test agent"), Agent)) { return false; }
        TestWorld.Population()->SetSharedInput(FVector2D(1, 0));
        TestWorld.Step(FrameRate * 2, 1.f / FrameRate);
        const double Distance = Agent->GetActorLocation().X;
        if (ReferenceDistance == 0) { ReferenceDistance = Distance; }
        TestTrue(FString::Printf(TEXT("%d FPS free travel is within 5%% of 30 FPS"), FrameRate),
            Distance > 300 && FMath::Abs(Distance - ReferenceDistance) < ReferenceDistance * 0.05);
        AddInfo(FString::Printf(TEXT("%d FPS: %.2f cm in 2 seconds"), FrameRate, Distance));
    }
    return true;
}

// 蓝图兼容测试：BP_CrowdAgent 可被原生生成入口创建，并保留/应用蓝图材质默认值。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdBlueprintSpawnTest, "Crowd.M0.BlueprintSpawnAppearance",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCrowdBlueprintSpawnTest::RunTest(const FString& Parameters)
{
    CrowdTests::FTestWorld TestWorld;
    UClass* AgentClass = LoadClass<ACrowdAgent>(nullptr,
        TEXT("/Game/GGJ/Crowd/Blueprints/Agents/BP_CrowdAgent.BP_CrowdAgent_C"));
    if (!TestNotNull(TEXT("Prototype Blueprint loads"), AgentClass)) { return false; }
    ACrowdAgent* Agent = TestWorld.Population()->SpawnAgent(AgentClass, FVector(0, 0, 33), FCrowdPhysicsSettings());
    if (!TestNotNull(TEXT("Blueprint agent spawns"), Agent)) { return false; }
    TestEqual(TEXT("Spawn retains Blueprint class"), Agent->GetClass(), AgentClass);
    TestNotNull(TEXT("Body palette setting inherited"), Agent->BodyMaterial.Get());
    TestNotNull(TEXT("Head palette setting inherited"), Agent->HeadMaterial.Get());
    TestEqual(TEXT("Runtime body material applied"), Agent->VisualMesh->GetMaterial(0), Agent->BodyMaterial.Get());
    TestEqual(TEXT("Runtime head material applied"), Agent->HeadMesh->GetMaterial(0), Agent->HeadMaterial.Get());
    TestTrue(TEXT("Blueprint body simulates physics"), Agent->Body->IsSimulatingPhysics());
    return true;
}

// 相机姿态测试：必须是透视斜视角，四次 90° 后回原位，轨道圆心和半径始终不变。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdOrbitPoseTest, "Crowd.Camera.PerspectiveAndFourViews",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCrowdOrbitPoseTest::RunTest(const FString& Parameters)
{
    CrowdTests::FTestWorld TestWorld;
    ACrowdCameraPawn* Camera = TestWorld.World->SpawnActor<ACrowdCameraPawn>();
    const FVector Center(430, -250, 70);
    Camera->ConfigureView(Center);
    const FVector FirstPosition = Camera->Camera->GetComponentLocation();
    for (int32 View = 0; View < 4; ++View)
    {
        TestEqual(TEXT("Camera uses perspective"), Camera->Camera->ProjectionMode.GetValue(), ECameraProjectionMode::Perspective);
        const FVector ToCenter = Center - Camera->Camera->GetComponentLocation();
        TestTrue(TEXT("Constant orbit radius"), FMath::IsNearlyEqual(ToCenter.Size(), static_cast<double>(Camera->OrbitSettings.OrbitDistance), 0.01));
        TestTrue(TEXT("Camera looks at arena center"), FVector::DotProduct(ToCenter.GetSafeNormal(), Camera->Camera->GetForwardVector()) > 0.999);
        TestTrue(TEXT("Camera is above ground and oblique"), Camera->Camera->GetComponentLocation().Z > Center.Z + 100.f && ToCenter.Size2D() > 100.f);
        const FVector2D Right = Camera->ScreenInputToWorld(FVector2D(1, 0));
        const FVector2D Up = Camera->ScreenInputToWorld(FVector2D(0, 1));
        TestTrue(TEXT("Input axes remain perpendicular unit vectors"), FMath::Abs(FVector2D::DotProduct(Right, Up)) < 0.001 && FMath::IsNearlyEqual(Right.Size(), 1.0, 0.001));
        TestTrue(TEXT("Quarter turn accepted"), Camera->RequestQuarterTurn(1));
        Camera->AdvanceOrbit(Camera->OrbitSettings.TurnDuration);
    }
    TestTrue(TEXT("Four turns return to initial position"), Camera->Camera->GetComponentLocation().Equals(FirstPosition, 0.01));
    TestTrue(TEXT("Pivot never moves with the orbit"), Camera->GetActorLocation().Equals(Center));
    return true;
}

// 相机缓动测试：起止减速、中点 45°、忙碌请求拒绝、Q/E 精确互逆且无过冲。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdOrbitEaseTest, "Crowd.Camera.EaseInOutAndBusyInput",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCrowdOrbitEaseTest::RunTest(const FString& Parameters)
{
    CrowdTests::FTestWorld TestWorld;
    ACrowdCameraPawn* Camera = TestWorld.World->SpawnActor<ACrowdCameraPawn>();
    FCrowdCameraSettings Settings;
    Settings.InitialYaw = 0.f;
    Settings.TurnDuration = 1.f;
    Camera->ApplySettings(Settings);
    TestFalse(TEXT("Zero direction rejected"), Camera->RequestQuarterTurn(0));
    TestTrue(TEXT("Q direction begins turn"), Camera->RequestQuarterTurn(1));
    TestFalse(TEXT("Busy turn does not accept an extra E"), Camera->RequestQuarterTurn(-1));
    Camera->AdvanceOrbit(0.1f);
    const float EarlyAngle = Camera->GetCurrentYaw();
    TestTrue(TEXT("Slow initial motion"), EarlyAngle > 0.f && EarlyAngle < 9.f);
    Camera->AdvanceOrbit(0.4f);
    TestTrue(TEXT("Halfway angle is 45 degrees"), FMath::IsNearlyEqual(Camera->GetCurrentYaw(), 45.f, 0.001f));
    Camera->AdvanceOrbit(0.4f);
    TestTrue(TEXT("Symmetric slow ending"), FMath::IsNearlyEqual(90.f - Camera->GetCurrentYaw(), EarlyAngle, 0.001f));
    Camera->AdvanceOrbit(0.11f);
    TestFalse(TEXT("Turn completes"), Camera->IsRotating());
    TestTrue(TEXT("Exactly 90 degrees without overshoot"), FMath::IsNearlyEqual(Camera->GetCurrentYaw(), 90.f, 0.001f));
    Camera->RequestQuarterTurn(-1);
    Camera->AdvanceOrbit(2.f);
    TestTrue(TEXT("E direction returns to initial view"), FMath::IsNearlyZero(Camera->GetCurrentYaw(), 0.001f));
    return true;
}

// 角度边界测试：跨越 ±180° 不反向，且 30/60/120 FPS 最终都精确转过 90°。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdOrbitFrameRateTest, "Crowd.Camera.WrapAndFrameRate",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCrowdOrbitFrameRateTest::RunTest(const FString& Parameters)
{
    CrowdTests::FTestWorld TestWorld;
    ACrowdCameraPawn* Camera = TestWorld.World->SpawnActor<ACrowdCameraPawn>();
    FCrowdCameraSettings Settings;
    Settings.InitialYaw = 170.f;
    for (const int32 Rate : {30, 60, 120})
    {
        Camera->ApplySettings(Settings);
        Camera->RequestQuarterTurn(1);
        float Travel = 0.f;
        float PreviousYaw = Camera->GetCurrentYaw();
        for (int32 Index = 0; Index < Rate; ++Index)
        {
            Camera->AdvanceOrbit(1.f / Rate);
            const float Delta = FMath::FindDeltaAngleDegrees(PreviousYaw, Camera->GetCurrentYaw());
            TestTrue(TEXT("Crossing 180 never reverses direction"), Delta >= -0.001f);
            Travel += Delta;
            PreviousYaw = Camera->GetCurrentYaw();
        }
        TestTrue(TEXT("Same 90-degree total travel at every frame rate"), FMath::IsNearlyEqual(Travel, 90.f, 0.01f));
        TestTrue(TEXT("Wrapped final yaw is -100 degrees"), FMath::IsNearlyEqual(Camera->GetCurrentYaw(), -100.f, 0.01f));
    }
    return true;
}

// 完整 Q/E 链路测试：输入触发转镜、移动方向随镜头改变、暂停冻结且 Q 可回到原观察面。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrowdOrbitInputTest, "Crowd.Camera.EnhancedInputAndMovementBasis",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCrowdOrbitInputTest::RunTest(const FString& Parameters)
{
    CrowdTests::FTestWorld TestWorld;
    ACrowdPlayerController* Controller = TestWorld.World->SpawnActor<ACrowdPlayerController>();
    ACrowdCameraPawn* Camera = TestWorld.World->SpawnActor<ACrowdCameraPawn>();
    FCrowdCameraSettings Settings;
    Settings.InitialYaw = -90.f;
    Camera->ApplySettings(Settings);
    Controller->Possess(Camera);
    UEnhancedPlayerInput* Input = NewObject<UEnhancedPlayerInput>(Controller);
    Controller->PlayerInput = Input;
    Controller->InputComponent = NewObject<UEnhancedInputComponent>(Controller);
    Controller->SetupInputComponent();
    TArray<UInputComponent*> Stack { Controller->InputComponent };
    Input->InjectInputForAction(Controller->RotateRightAction, FInputActionValue(true));
    Input->ProcessInputStack(Stack, 1.f / 60.f, false);
    TestTrue(TEXT("E action starts camera rotation"), Camera->IsRotating());
    for (int32 Frame = 0; Frame < 60; ++Frame)
    {
        Input->InjectInputForAction(Controller->MoveAction, FInputActionValue(FVector2D(0, 1)));
        Input->ProcessInputStack(Stack, 1.f / 60.f, false);
    }
    TestTrue(TEXT("After E, screen-up moves -X instead of -Y"), TestWorld.Population()->GetSharedInput().Equals(FVector2D(-1, 0), 0.001));
    Input->InjectInputForAction(Controller->RotateLeftAction, FInputActionValue(true));
    Input->ProcessInputStack(Stack, 1.f / 60.f, false);
    const float PausedYaw = Camera->GetCurrentYaw();
    Input->ProcessInputStack(Stack, 0.5f, true);
    TestTrue(TEXT("Paused camera does not advance"), FMath::IsNearlyEqual(Camera->GetCurrentYaw(), PausedYaw));
    TestTrue(TEXT("Paused movement clears"), TestWorld.Population()->GetSharedInput().IsNearlyZero());
    for (int32 Frame = 0; Frame < 60; ++Frame) { Input->ProcessInputStack(Stack, 1.f / 60.f, false); }
    TestTrue(TEXT("Q returns to the original heading"), FMath::IsNearlyEqual(Camera->GetCurrentYaw(), -90.f, 0.001f));
    return true;
}

#endif
