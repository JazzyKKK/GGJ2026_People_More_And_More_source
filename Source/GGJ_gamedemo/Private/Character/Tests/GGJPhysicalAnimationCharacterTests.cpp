// Physical Character 的资源兼容与运行时物理自动化测试。
#if WITH_DEV_AUTOMATION_TESTS

#include "Character/GGJPhysicalAnimationCharacter.h"
#include "Character/GGJCharacterGroupManager.h"
#include "Character/GGJGroupCameraActor.h"
#include "Gameplay/Zones/GGJCountZone.h"
#include "Gameplay/Zones/GGJDestroyZone.h"
#include "Gameplay/Zones/GGJFlowZone.h"
#include "Gameplay/Zones/GGJSpawnPacking.h"
#include "Gameplay/Zones/GGJSpawnZone.h"
#include "Animation/AnimSequence.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/AutomationTest.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/PhysicalAnimationComponent.h"
#include "PhysicsEngine/SkeletalBodySetup.h"

namespace PhysicalCharacterTests
{
    /** 为运行测试建立有 Chaos、无渲染/音频负担的瞬态 Game World。 */
    struct FTestWorld
    {
        UWorld* World = nullptr;

        FTestWorld()
        {
            UWorld::InitializationValues Values;
            Values.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false)
                .CreatePhysicsScene(true).ShouldSimulatePhysics(true).EnableTraceCollision(true)
                .SetTransactional(false).CreateFXSystem(false);
            World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
                ERHIFeatureLevel::Num, &Values);
            GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
            World->InitializeActorsForPlay(FURL());
            World->GetWorldSettings()->NotifyBeginPlay();
            World->GetWorldSettings()->NotifyMatchStarted();
            World->BeginPlay();

            // 角色脚下放一个静态碰撞地板，确保模拟骨体不会无限下落。
            //AActor* Floor = World->SpawnActor<AActor>();
            //UBoxComponent* Box = NewObject<UBoxComponent>(Floor);
            //Floor->SetRootComponent(Box);
            //Box->SetBoxExtent(FVector(600.f, 600.f, 20.f));
            //Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
            //Box->SetCollisionObjectType(ECC_WorldStatic);
            //Box->SetCollisionResponseToAllChannels(ECR_Block);
            //Box->SetWorldLocation(FVector(0.f, 0.f, -20.f));
            //Box->RegisterComponent();
        }

        ~FTestWorld()
        {
            World->EndPlay(EEndPlayReason::Quit);
            GEngine->DestroyWorldContext(World);
            World->DestroyWorld(false);
        }

        void Step(const int32 Frames, const float DeltaTime = 1.f / 60.f) const
        {
            for (int32 Index = 0; Index < Frames; ++Index)
            {
                World->Tick(LEVELTICK_All, DeltaTime);
                ++GFrameCounter;
            }
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPhysicalCharacterDefaultsTest,
    "PhysicalCharacter.Configuration.MatchesTestLabBlueprint",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPhysicalCharacterDefaultsTest::RunTest(const FString& Parameters)
{
    const AGGJPhysicalAnimationCharacter* Defaults = GetDefault<AGGJPhysicalAnimationCharacter>();
    if (!TestNotNull(TEXT("Native character CDO"), Defaults)) { return false; }

    TestNotNull(TEXT("Physical Animation component"),
        Defaults->FindComponentByClass<UPhysicalAnimationComponent>());
    TestNotNull(TEXT("400 cm third-person spring arm"), Defaults->GetCameraBoom());
    TestNotNull(TEXT("Follow camera"), Defaults->GetFollowCamera());
    TestTrue(TEXT("Official camera distance is 400"),
        FMath::IsNearlyEqual(Defaults->GetCameraBoom()->TargetArmLength, 400.f));
    TestEqual(TEXT("Imported Mixamo driven bone"), Defaults->DrivenBoneName, FName(TEXT("Hips")));
    TestFalse(TEXT("Hips remains kinematic like Test_Lab2"), Defaults->bIncludeDrivenBoneInSimulation);
    TestTrue(TEXT("PhysicalAnimation strength multiplier is 5"), FMath::IsNearlyEqual(Defaults->MuscleStrength, 5.f));
    TestTrue(TEXT("Orientation strength copied"), FMath::IsNearlyEqual(Defaults->DriveSettings.OrientationStrength, 100.f));
    TestTrue(TEXT("Angular velocity strength copied"), FMath::IsNearlyEqual(Defaults->DriveSettings.AngularVelocityStrength, 100.f));
    TestTrue(TEXT("Position strength copied"), FMath::IsNearlyEqual(Defaults->DriveSettings.PositionStrength, 100.f));
    TestTrue(TEXT("Velocity strength copied"), FMath::IsNearlyEqual(Defaults->DriveSettings.VelocityStrength, 100.f));
    TestTrue(TEXT("Unlimited linear force copied"), FMath::IsNearlyZero(Defaults->DriveSettings.MaxLinearForce));
    TestTrue(TEXT("Unlimited angular force copied"), FMath::IsNearlyZero(Defaults->DriveSettings.MaxAngularForce));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPhysicalCharacterAssetCompatibilityTest,
    "PhysicalCharacter.Assets.SharedSkeletonAndPhysicsAsset",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPhysicalCharacterAssetCompatibilityTest::RunTest(const FString& Parameters)
{
    UClass* BlueprintClass = LoadClass<AGGJPhysicalAnimationCharacter>(nullptr,
        TEXT("/Game/GGJ/Character/Blueprints/BP_PhysicalAnimationCharacter.BP_PhysicalAnimationCharacter_C"));
    if (!TestNotNull(TEXT("Character Blueprint class"), BlueprintClass)) { return false; }

    const AGGJPhysicalAnimationCharacter* Defaults = Cast<AGGJPhysicalAnimationCharacter>(BlueprintClass->GetDefaultObject());
    if (!TestNotNull(TEXT("Character Blueprint CDO"), Defaults)) { return false; }
    USkeletalMeshComponent* MeshComponent = Defaults->GetMesh();
    if (!TestNotNull(TEXT("Configured skeletal mesh"), MeshComponent->GetSkeletalMeshAsset())) { return false; }
    if (!TestNotNull(TEXT("Configured skeleton"), MeshComponent->GetSkeletalMeshAsset()->GetSkeleton())) { return false; }
    if (!TestNotNull(TEXT("Configured PhysicsAsset"), MeshComponent->GetPhysicsAsset())) { return false; }

    // 这批 FBX 的单位标签错误曾让模型放大 100 倍；限制合理高度，防止重新导入时回归。
    const float MeshHeightCm = MeshComponent->GetSkeletalMeshAsset()->GetBounds().BoxExtent.Z * 2.f;
    TestTrue(TEXT("Imported mesh has a playable centimeter scale"),
        MeshHeightCm >= 100.f && MeshHeightCm <= 500.f);

    USkeleton* Skeleton = MeshComponent->GetSkeletalMeshAsset()->GetSkeleton();
    for (const TPair<const TCHAR*, UAnimationAsset*> Pair : {
        TPair<const TCHAR*, UAnimationAsset*>(TEXT("Idle"), Defaults->IdleAnimation),
        TPair<const TCHAR*, UAnimationAsset*>(TEXT("Walk"), Defaults->WalkAnimation),
        TPair<const TCHAR*, UAnimationAsset*>(TEXT("Run"), Defaults->RunAnimation)})
    {
        const UAnimSequence* Sequence = Cast<UAnimSequence>(Pair.Value);
        if (!TestNotNull(FString::Printf(TEXT("%s AnimSequence"), Pair.Key), Sequence)) { return false; }
        if (Sequence->GetSkeleton() != Skeleton)
        {
            AddWarning(FString::Printf(TEXT("%s animation '%s' uses Skeleton '%s', expected '%s'; runtime will keep Idle until it is retargeted."),
                Pair.Key, *GetNameSafe(Sequence), *GetNameSafe(Sequence->GetSkeleton()), *GetNameSafe(Skeleton)));
        }
    }

    TestTrue(TEXT("Driven bone exists"), MeshComponent->GetBoneIndex(Defaults->DrivenBoneName) != INDEX_NONE);
    TestTrue(TEXT("Auto PhysicsAsset has multiple bodies"),
        MeshComponent->GetPhysicsAsset()->SkeletalBodySetups.Num() >= 5);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPhysicalCharacterRuntimeTest,
    "PhysicalCharacter.Runtime.ActiveRagdollStartsAndResets",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPhysicalCharacterRuntimeTest::RunTest(const FString& Parameters)
{
    UClass* BlueprintClass = LoadClass<AGGJPhysicalAnimationCharacter>(nullptr,
        TEXT("/Game/GGJ/Character/Blueprints/BP_PhysicalAnimationCharacter.BP_PhysicalAnimationCharacter_C"));
    if (!TestNotNull(TEXT("Character Blueprint class"), BlueprintClass)) { return false; }

    PhysicalCharacterTests::FTestWorld TestWorld;
    AGGJPhysicalAnimationCharacter* Character = TestWorld.World->SpawnActor<AGGJPhysicalAnimationCharacter>(
        BlueprintClass, FVector(0.f, 0.f, 120.f), FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("Runtime character spawned"), Character)) { return false; }
    TestWorld.Step(8);

    TestTrue(TEXT("BeginPlay initialized Physical Animation"), Character->IsPhysicalAnimationReady());
    USkeletalMeshComponent* MeshComponent = Character->GetMesh();
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
    TestTrue(TEXT("At least one child body simulates"), SimulatingBodies > 0);
    if (const FBodyInstance* HipBody = MeshComponent->GetBodyInstance(Character->DrivenBoneName))
    {
        TestFalse(TEXT("Hips body remains kinematic"), HipBody->IsInstanceSimulatingPhysics());
    }

    // 不只检查组件状态：持续输入一秒，确认 CharacterMovement 与物理动画同时启用时仍可操控。
    APlayerController* Controller = TestWorld.World->SpawnActor<APlayerController>();
    TestNotNull(TEXT("Controller spawned"), Controller);
    Controller->Possess(Character);
    const FVector MovementStart = Character->GetActorLocation();
    for (int32 Frame = 0; Frame < 60; ++Frame)
    {
        Character->DoMove(0.f, 1.f);
        TestWorld.Step(1);
    }
    TestTrue(TEXT("Character moves under continuous forward input"),
        FVector::Dist2D(MovementStart, Character->GetActorLocation()) > 100.f);

    Character->SetMuscleStrength(0.25f);
    UPhysicalAnimationComponent* Physical = Character->FindComponentByClass<UPhysicalAnimationComponent>();
    TestTrue(TEXT("Muscle strength changes at runtime"),
        Physical && FMath::IsNearlyEqual(Physical->StrengthMultiplyer, 0.25f));
    Character->ResetPhysicalAnimation();
    TestWorld.Step(3);
    TestTrue(TEXT("Reset reinitializes active ragdoll"), Character->IsPhysicalAnimationReady());
    TestFalse(TEXT("Physics keeps a finite transform"), MeshComponent->GetComponentTransform().ContainsNaN());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPhysicalCharacterGroupControlTest,
    "PhysicalCharacter.Group.InitialThreeAndSharedControl",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPhysicalCharacterGroupControlTest::RunTest(const FString& Parameters)
{
    // 群体测试只验证名册和同步 CharacterMovement。单人物理动画已由上一项测试覆盖，
    // 因此这里使用不加载美术资产的原生类，让测试专注于群体管理行为。
    PhysicalCharacterTests::FTestWorld TestWorld;
    AGGJPhysicalAnimationCharacter* Leader = TestWorld.World->SpawnActor<AGGJPhysicalAnimationCharacter>(
        AGGJPhysicalAnimationCharacter::StaticClass(), FVector(-300.f, 0.f, 120.f), FRotator::ZeroRotator);
    APlayerController* Controller = TestWorld.World->SpawnActor<APlayerController>();
    if (!TestNotNull(TEXT("Leader"), Leader) || !TestNotNull(TEXT("Controller"), Controller))
    {
        return false;
    }
    Controller->Possess(Leader);

    AGGJCharacterGroupManager* Manager = TestWorld.World->SpawnActor<AGGJCharacterGroupManager>();
    if (!TestNotNull(TEXT("Group Manager"), Manager))
    {
        return false;
    }
    TestWorld.Step(12);

    TestEqual(TEXT("Initial population is three"), Manager->GetPopulationCount(), 3);
    TestTrue(TEXT("Possessed pawn is the Leader"), Manager->GetLeader() == Leader);
    const TArray<AGGJPhysicalAnimationCharacter*> Members = Manager->GetMembers();
    if (!TestEqual(TEXT("Three valid member references"), Members.Num(), 3))
    {
        return false;
    }

    TMap<AGGJPhysicalAnimationCharacter*, FVector> Starts;
    for (AGGJPhysicalAnimationCharacter* Member : Members)
    {
        Starts.Add(Member, Member->GetActorLocation());
        TestTrue(TEXT("Every member points back to the same Manager"),
            Member->GetCharacterGroupManager() == Manager);
    }

    Leader->DoJumpStart();
    for (const AGGJPhysicalAnimationCharacter* Member : Members)
    {
        TestTrue(TEXT("Leader jump press is broadcast to every member"), Member->bPressedJump);
    }
    Leader->DoJumpEnd();
    for (const AGGJPhysicalAnimationCharacter* Member : Members)
    {
        TestFalse(TEXT("Leader jump release is broadcast to every member"), Member->bPressedJump);
    }

    for (int32 Frame = 0; Frame < 60; ++Frame)
    {
        Manager->ApplySharedMovement(FVector::ForwardVector);
        TestWorld.Step(1);
    }
    for (AGGJPhysicalAnimationCharacter* Member : Members)
    {
        TestTrue(TEXT("Every member responds to the shared input"),
            FVector::Dist2D(Starts.FindChecked(Member), Member->GetActorLocation()) > 100.f);
    }

    TestEqual(TEXT("One follower can be removed"), Manager->RemovePopulation(1), 1);
    TestEqual(TEXT("Leader is retained after removal"), Manager->GetPopulationCount(), 2);
    TestTrue(TEXT("Leader remains registered"), Manager->GetLeader() == Leader);
    TestEqual(TEXT("Reserved growth interface can add one"),
        Manager->RequestPopulationDelta(1, Leader->GetActorLocation()), 1);
    TestEqual(TEXT("Population returns to three"), Manager->GetPopulationCount(), 3);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPhysicalCharacterGroupCameraTest,
    "PhysicalCharacter.Camera.TracksFramesAndQuarterTurns",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPhysicalCharacterGroupCameraTest::RunTest(const FString& Parameters)
{
    PhysicalCharacterTests::FTestWorld TestWorld;
    AGGJPhysicalAnimationCharacter* Leader = TestWorld.World->SpawnActor<AGGJPhysicalAnimationCharacter>(
        AGGJPhysicalAnimationCharacter::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator);
    APlayerController* Controller = TestWorld.World->SpawnActor<APlayerController>();
    AGGJCharacterGroupManager* Manager = TestWorld.World->SpawnActor<AGGJCharacterGroupManager>();
    AGGJGroupCameraActor* Camera = TestWorld.World->SpawnActor<AGGJGroupCameraActor>();
    if (!TestNotNull(TEXT("Leader"), Leader) || !TestNotNull(TEXT("Controller"), Controller)
        || !TestNotNull(TEXT("Manager"), Manager) || !TestNotNull(TEXT("Group camera"), Camera))
    {
        return false;
    }

    Controller->Possess(Leader);
    Manager->SetGroupCamera(Camera);
    Camera->ActivateForPlayer(Controller, Manager, Leader);
    TestWorld.Step(12);
    TestTrue(TEXT("Camera is the view target"), Controller->GetViewTarget() == Camera);
    TestEqual(TEXT("Manager exposes camera"), Manager->GetGroupCamera(), Camera);
    TestTrue(TEXT("Initial distance respects minimum"), Camera->GetCurrentDistance() >= Camera->MinDistance);

    const FVector InputDirection = Camera->ScreenInputToWorld(FVector2D(0.f, 1.f));
    TestFalse(TEXT("Camera-relative input is finite"), InputDirection.ContainsNaN());
    TestTrue(TEXT("Camera-relative input is normalized"), InputDirection.Size2D() <= 1.001f);

    const float StartYaw = Camera->GetCurrentYaw();
    Leader->RotateGroupCameraLeft();
    TestTrue(TEXT("Q/left action starts the positive-yaw quarter turn"), Camera->GetCurrentYaw() == StartYaw);
    TestFalse(TEXT("A second turn is rejected during transition"), Camera->RequestQuarterTurn(1));
    TestWorld.Step(60);
    TestTrue(TEXT("Q direction rotates exactly 90 degrees"),
        FMath::IsNearlyEqual(FMath::FindDeltaAngleDegrees(StartYaw, Camera->GetCurrentYaw()), 90.f, 0.1f));

    const float ClusteredDistance = Camera->GetCurrentDistance();
    const TArray<AGGJPhysicalAnimationCharacter*> Members = Manager->GetMembers();
    if (!TestEqual(TEXT("Camera test has initial three members"), Members.Num(), 3)) { return false; }
    Members.Last()->SetActorLocation(FVector(3600.f, 0.f, 0.f));
    TestWorld.Step(90);
    TestTrue(TEXT("Camera focus follows the group center"), Camera->GetCurrentFocus().X > 500.f);
    TestTrue(TEXT("Camera zooms out as the group spreads"),
        Camera->GetCurrentDistance() > ClusteredDistance + 100.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPhysicalCharacterSequentialRuntimeSpawnTest,
    "PhysicalCharacter.Group.SequentialRuntimeSpawn",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPhysicalCharacterSequentialRuntimeSpawnTest::RunTest(const FString& Parameters)
{
    PhysicalCharacterTests::FTestWorld TestWorld;
    AGGJPhysicalAnimationCharacter* Leader = TestWorld.World->SpawnActor<AGGJPhysicalAnimationCharacter>(
        AGGJPhysicalAnimationCharacter::StaticClass(), FVector(0.f, 0.f, 120.f), FRotator::ZeroRotator);
    APlayerController* Controller = TestWorld.World->SpawnActor<APlayerController>();
    Controller->Possess(Leader);
    AGGJCharacterGroupManager* Manager = TestWorld.World->SpawnActor<AGGJCharacterGroupManager>();
    Manager->MaxPopulation = 10;
    Manager->RuntimeSpawnInterval = 0.05f;
    TestWorld.Step(20);

    const int32 PopulationBefore = Manager->GetPopulationCount();
    TestEqual(TEXT("A runtime batch reserves all three available requests"),
        Manager->AddPopulation(3, FVector(1000.f, 0.f, 500.f)), 3);
    TestEqual(TEXT("Only the first queued member appears immediately"),
        Manager->GetPopulationCount(), PopulationBefore + 1);
    TestEqual(TEXT("The rest of the batch remains queued"),
        Manager->GetPendingPopulationCount(), 2);

    TestWorld.Step(8);
    TestEqual(TEXT("Queued members spawn one at a time until the batch is complete"),
        Manager->GetPopulationCount(), PopulationBefore + 3);
    TestEqual(TEXT("Runtime spawn queue becomes empty"),
        Manager->GetPendingPopulationCount(), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPhysicalCharacterSpawnZoneDistributionTest,
    "PhysicalCharacter.Zones.SpawnZonesDistributeInitialPopulation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPhysicalCharacterSpawnZoneDistributionTest::RunTest(const FString& Parameters)
{
    PhysicalCharacterTests::FTestWorld TestWorld;
    AGGJSpawnZone* LeftZone = TestWorld.World->SpawnActor<AGGJSpawnZone>(
        AGGJSpawnZone::StaticClass(), FVector(-1200.f, 0.f, 120.f), FRotator::ZeroRotator);
    AGGJSpawnZone* RightZone = TestWorld.World->SpawnActor<AGGJSpawnZone>(
        AGGJSpawnZone::StaticClass(), FVector(1200.f, 0.f, 120.f), FRotator::ZeroRotator);
    LeftZone->SpawnOrder = 0;
    RightZone->SpawnOrder = 1;

    AGGJPhysicalAnimationCharacter* Leader = TestWorld.World->SpawnActor<AGGJPhysicalAnimationCharacter>();
    APlayerController* Controller = TestWorld.World->SpawnActor<APlayerController>();
    Controller->Possess(Leader);
    AGGJCharacterGroupManager* Manager = TestWorld.World->SpawnActor<AGGJCharacterGroupManager>();
    Manager->InitialPopulation = 5;
    Manager->MaxPopulation = 10;
    TestWorld.Step(12);

    const TArray<AGGJPhysicalAnimationCharacter*> Members = Manager->GetMembers();
    TestEqual(TEXT("Five initial members were created"), Members.Num(), 5);
    int32 LeftCount = 0;
    int32 RightCount = 0;
    for (const AGGJPhysicalAnimationCharacter* Member : Members)
    {
        if (Member->GetActorLocation().X < 0.f) { ++LeftCount; }
        else { ++RightCount; }
    }
    TestEqual(TEXT("Remainder goes to first spawn zone"), LeftCount, 3);
    TestEqual(TEXT("Second spawn zone receives its even share"), RightCount, 2);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPhysicalCharacterCompactSpawnPackingTest,
    "PhysicalCharacter.Zones.CompactSpawnPacking",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPhysicalCharacterCompactSpawnPackingTest::RunTest(const FString& Parameters)
{
    constexpr float Spacing = 145.f;
    constexpr float LayerHeight = 145.f;
    constexpr int32 LayerCapacity = 4;
    constexpr int32 Count = 9;

    for (int32 Index = 0; Index < Count; ++Index)
    {
        const FVector Offset = GGJSpawnPacking::MakeCompactStackOffset(
            Index, Count, Spacing, LayerCapacity, LayerHeight, 0.f);
        const int32 ExpectedLayer = Index / LayerCapacity;
        TestTrue(TEXT("Spawn height follows compact layer index"),
            FMath::IsNearlyEqual(Offset.Z, ExpectedLayer * LayerHeight));
        TestTrue(TEXT("Each compact layer stays close to its center"),
            FMath::Abs(Offset.X) <= Spacing * 0.5f
            && FMath::Abs(Offset.Y) <= Spacing * 0.5f);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPhysicalCharacterFlowZoneTest,
    "PhysicalCharacter.Zones.FlowDirectionAndVelocity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPhysicalCharacterFlowZoneTest::RunTest(const FString& Parameters)
{
    PhysicalCharacterTests::FTestWorld TestWorld;
    AGGJPhysicalAnimationCharacter* Character =
        TestWorld.World->SpawnActor<AGGJPhysicalAnimationCharacter>();
    AGGJFlowZone* Zone = TestWorld.World->SpawnActor<AGGJFlowZone>(
        AGGJFlowZone::StaticClass(), FVector::ZeroVector, FRotator(0.f, 90.f, 0.f));
    if (!TestNotNull(TEXT("Flow zone"), Zone)
        || !TestNotNull(TEXT("Flow test character"), Character))
    {
        return false;
    }

    UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
    Movement->Velocity = FVector::ZeroVector;
    Zone->FlowMode = EGGJFlowZoneMode::TargetVelocity;
    Zone->FlowSpeed = 600.f;
    Zone->Acceleration = 1200.f;
    Zone->ApplyFlowToCharacter(Character, 0.5f);
    TestTrue(TEXT("Rotating the zone rotates conveyor velocity toward world +Y"),
        Movement->Velocity.Y > 599.f && FMath::Abs(Movement->Velocity.X) < 1.f);
    TestTrue(TEXT("Ground movement also receives forced input so braking cannot erase the flow"),
        Character->GetPendingMovementInputVector().Y > 0.f);

    Movement->Velocity = FVector::ZeroVector;
    Zone->FlowMode = EGGJFlowZoneMode::Acceleration;
    Zone->FlowSpeed = 1000.f;
    Zone->Acceleration = 200.f;
    Zone->ApplyFlowToCharacter(Character, 0.5f);
    TestTrue(TEXT("Wind mode adds acceleration without instantly reaching its cap"),
        FMath::IsNearlyEqual(Movement->Velocity.Y, 100.f, 0.1f));

    Zone->SetActorRotation(FRotator(90.f, 0.f, 0.f));
    Zone->bUseFull3DDirection = true;
    Zone->bAutoLiftGroundedCharacters = true;
    Zone->AutoLiftMinimumDirectionZ = 0.1f;
    Zone->FlowSpeed = 650.f;
    Zone->Acceleration = 1600.f;
    Movement->Velocity = FVector::ZeroVector;
    Movement->SetMovementMode(MOVE_Walking);
    Zone->ApplyFlowToCharacter(Character, 0.1f);
    TestTrue(TEXT("Upward flow automatically releases a grounded character"),
        Movement->MovementMode == MOVE_Falling);
    TestTrue(TEXT("Upward flow gives positive vertical velocity without jump input"),
        Movement->Velocity.Z > 0.f);

    Zone->bAutoLiftGroundedCharacters = false;
    Movement->Velocity = FVector::ZeroVector;
    Movement->SetMovementMode(MOVE_Walking);
    Zone->ApplyFlowToCharacter(Character, 0.1f);
    TestTrue(TEXT("Automatic lift can be disabled for sloped conveyors"),
        Movement->MovementMode == MOVE_Walking);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPhysicalCharacterCountAndDestroyZoneTest,
    "PhysicalCharacter.Zones.CountAndDestroy",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPhysicalCharacterCountAndDestroyZoneTest::RunTest(const FString& Parameters)
{
    PhysicalCharacterTests::FTestWorld TestWorld;
    AGGJPhysicalAnimationCharacter* Leader = TestWorld.World->SpawnActor<AGGJPhysicalAnimationCharacter>(
        AGGJPhysicalAnimationCharacter::StaticClass(), FVector(0.f, 0.f, 120.f), FRotator::ZeroRotator);
    APlayerController* Controller = TestWorld.World->SpawnActor<APlayerController>();
    Controller->Possess(Leader);
    AGGJCharacterGroupManager* Manager = TestWorld.World->SpawnActor<AGGJCharacterGroupManager>();
    TestWorld.Step(12);

    AGGJCountZone* CountZone = TestWorld.World->SpawnActor<AGGJCountZone>();
    CountZone->CountBounds->SetBoxExtent(FVector(2000.f, 2000.f, 2000.f));
    // 明确关闭碰撞，证明计数依靠几何检测而不是偶然收到 Overlap。
    CountZone->CountBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    CountZone->CountBounds->SetGenerateOverlapEvents(false);
    CountZone->Comparison = EGGJCountComparison::Equal;
    CountZone->TargetCount = 3;
    CountZone->CountBounds->UpdateOverlaps();
    TestWorld.Step(3);
    TestEqual(TEXT("Count zone reads scene population"), CountZone->GetScenePopulation(), 3);
    TestEqual(TEXT("Count zone counts unique members inside"), CountZone->GetInsidePopulation(), 3);
    TestTrue(TEXT("Equal comparison triggers at target"), CountZone->IsTriggered());

    CountZone->Comparison = EGGJCountComparison::Greater;
    TestTrue(TEXT("Built-in greater comparison works"),
        CountZone->EvaluateCountCondition(4, 5, 3));
    CountZone->Comparison = EGGJCountComparison::Custom;
    TestFalse(TEXT("Custom comparison is safe until Blueprint overrides it"),
        CountZone->EvaluateCountCondition(4, 5, 3));

    const TArray<AGGJPhysicalAnimationCharacter*> Members = Manager->GetMembers();
    AGGJPhysicalAnimationCharacter* Follower = Members.Num() > 1 ? Members[1] : nullptr;
    if (!TestNotNull(TEXT("Follower for destroy zone"), Follower)) { return false; }
    const FVector IsolatedDestroyLocation(10000.f, 0.f, 120.f);
    AGGJDestroyZone* DestroyZone = TestWorld.World->SpawnActor<AGGJDestroyZone>(
        AGGJDestroyZone::StaticClass(), IsolatedDestroyLocation, FRotator::ZeroRotator);
    // 紧凑排布后成员彼此很近；这里缩小测试区域，保持“只销毁指定成员”的测试语义。
    DestroyZone->DestroyBounds->SetBoxExtent(FVector(50.f));
    DestroyZone->DetectionPadding = 0.f;
    DestroyZone->DestroyBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    DestroyZone->DestroyBounds->SetGenerateOverlapEvents(false);
    // 把目标成员与紧凑人堆隔离，避免其他成员同时落入检测盒。
    Follower->SetActorLocation(IsolatedDestroyLocation, false, nullptr,
        ETeleportType::TeleportPhysics);
    TestWorld.Step(3);
    TestEqual(TEXT("Destroy zone removes overlapping follower"), Manager->GetPopulationCount(), 2);

    AGGJPhysicalAnimationCharacter* OldLeader = Manager->GetLeader();
    TestTrue(TEXT("Manager destroys current leader"), Manager->DestroyMember(OldLeader));
    TestEqual(TEXT("One member remains after leader destruction"), Manager->GetPopulationCount(), 1);
    TestTrue(TEXT("A remaining member is promoted"), Manager->GetLeader() != nullptr);
    TestTrue(TEXT("Controller is transferred to promoted leader"),
        Controller->GetPawn() == Manager->GetLeader());
    return true;
}

#endif
