// 单屏关卡的人口初始化、重置及镜头中心配置实现。
// 每张人群玩法地图必须恰好放置一个 Director。
#include "Crowd/Framework/CrowdLevelDirector.h"
#include "Crowd/Framework/CrowdCameraPawn.h"
#include "Crowd/Agents/CrowdAgent.h"
#include "Crowd/Agents/CrowdSpawnPoint.h"
#include "Crowd/Core/CrowdPopulationSubsystem.h"
#include "Components/ArrowComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

ACrowdLevelDirector::ACrowdLevelDirector()
{
    // Director 只在开始和手动 Reset 时工作，不需要逐帧 Tick。
    PrimaryActorTick.bCanEverTick = false;
    SpawnOrigin = CreateDefaultSubobject<UArrowComponent>(TEXT("SpawnOrigin"));
    SetRootComponent(SpawnOrigin);
    SpawnOrigin->ArrowColor = FColor::Green;
    AgentClass = ACrowdAgent::StaticClass();
}

void ACrowdLevelDirector::BeginPlay()
{
    Super::BeginPlay();
    // 延迟到下一帧：确保所有场景碰撞体和 GameMode 自动生成的 CameraPawn 已完成 BeginPlay。
    GetWorldTimerManager().SetTimerForNextTick(this, &ACrowdLevelDirector::ConfigureCamera);
    GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]() { ResetCrowd(); }));
}

void ACrowdLevelDirector::ConfigureCamera()
{
    // 当前为单机单视口玩法，只配置第一个本地 PlayerController 的 Pawn。
    if (APlayerController* Controller = GetWorld()->GetFirstPlayerController())
    {
        if (ACrowdCameraPawn* CameraPawn = Cast<ACrowdCameraPawn>(Controller->GetPawn()))
        {
            if (bOverrideCameraSettings)
            {
                CameraPawn->ApplySettings(CameraSettingsOverride);
            }
            // 有 Target 时它优先；否则使用 Director 位置加偏移作为轨道圆心。
            CameraPawn->ConfigureView(IsValid(CameraTarget) ? CameraTarget->GetActorLocation()
                : GetActorLocation() + CameraCenterOffset);
        }
        else
        {
            ReportError(TEXT("Set this map's GameMode Override to CrowdGameMode; remove auto-possessing old pawns."));
        }
    }
}

void ACrowdLevelDirector::ReportError(const FString& Message)
{
    // 日志便于程序定位，屏幕红字便于不看 Output Log 的关卡设计师立即发现问题。
    UE_LOG(LogTemp, Error, TEXT("Crowd: %s"), *Message);
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 15.f, FColor::Red, TEXT("Crowd: ") + Message);
    }
}

bool ACrowdLevelDirector::ResetCrowd()
{
    // 多个 Director 会争夺同一 WorldSubsystem；零个则没有初始化入口，所以必须恰好一个。
    int32 DirectorCount = 0;
    for (TActorIterator<ACrowdLevelDirector> It(GetWorld()); It; ++It) { ++DirectorCount; }
    if (DirectorCount != 1)
    {
        ReportError(TEXT("A crowd map must contain exactly one LevelDirector."));
        return false;
    }
    UCrowdPopulationSubsystem* Population = GetWorld()->GetSubsystem<UCrowdPopulationSubsystem>();
    if (!Population || !AgentClass)
    {
        ReportError(TEXT("Missing population subsystem or AgentClass."));
        return false;
    }
    // 重置采用“先清空、再完整重建”，并保证旧输入不会带到新人口。
    Population->ClearPopulation();
    bInitialized = false;
    const FCrowdPhysicsSettings Settings = (PhysicsData ? PhysicsData->Settings : DefaultPhysics).Sanitized();
    // SpawnPoints 一旦非空就决定人数；否则使用自动阵列人数。
    const int32 Count = SpawnPoints.IsEmpty() ? FMath::Clamp(InitialCount, 1, 64) : SpawnPoints.Num();
    if (Count > 64)
    {
        ReportError(TEXT("M0 supports at most 64 initial agents."));
        return false;
    }
    const int32 Columns = FMath::Clamp(GridColumns, 1, Count);
    const int32 Rows = FMath::DivideAndRoundUp(Count, Columns);
    // 给两个球之间留 6 cm 缓冲，防止浮点误差让出生重叠检测失败。
    const float Spacing = FMath::Max(GridSpacing, Settings.Radius * 2.f + 6.f);
    for (int32 Index = 0; Index < Count; ++Index)
    {
        FVector Location = GetActorLocation();
        if (!SpawnPoints.IsEmpty())
        {
            // 数组中任何空项都视为关卡配置错误，并回滚已经生成的部分人口。
            if (!IsValid(SpawnPoints[Index]))
            {
                Population->ClearPopulation();
                ReportError(FString::Printf(TEXT("SpawnPoints[%d] is empty. No partial population retained."), Index));
                return false;
            }
            Location.X = SpawnPoints[Index]->GetActorLocation().X;
            Location.Y = SpawnPoints[Index]->GetActorLocation().Y;
        }
        else
        {
            // 以 Director 为中心排布规则网格，而不是从一角向正方向延伸。
            Location.X += (Index % Columns - (Columns - 1) * 0.5f) * Spacing;
            Location.Y += (Index / Columns - (Rows - 1) * 0.5f) * Spacing;
        }
        // 出生点/Director 的 Z 表示地面；球心抬高“半径 + 3 cm”避免嵌入地板。
        Location.Z += Settings.Radius + 3.f;
        if (!Population->SpawnAgent(AgentClass, Location, Settings))
        {
            // 初始化是原子操作：玩家不会遇到人数不完整但状态显示成功的关卡。
            Population->ClearPopulation();
            ReportError(FString::Printf(TEXT("Spawn %d blocked at %s. Move spawn origin/markers away from walls."), Index, *Location.ToCompactString()));
            return false;
        }
    }
    bInitialized = true;
    UE_LOG(LogTemp, Display, TEXT("Crowd: initialized %d agents."), Population->GetPopulationCount());
    return true;
}
