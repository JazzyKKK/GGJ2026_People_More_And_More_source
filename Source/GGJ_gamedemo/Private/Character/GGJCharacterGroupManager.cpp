// 物理动画人物的名册、初始三人生成、统一输入和未来人数变化接口。
#include "Character/GGJCharacterGroupManager.h"

#include "Character/GGJGroupCameraActor.h"
#include "Character/GGJPhysicalAnimationCharacter.h"
#include "Gameplay/Zones/GGJSpawnZone.h"
#include "Gameplay/Zones/GGJSpawnPacking.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

AGGJCharacterGroupManager::AGGJCharacterGroupManager()
{
    PrimaryActorTick.bCanEverTick = false;
}

void AGGJCharacterGroupManager::SetGroupCamera(AGGJGroupCameraActor* NewCamera)
{
    GroupCamera = NewCamera;
}

void AGGJCharacterGroupManager::BeginPlay()
{
    Super::BeginPlay();
    ScheduleInitializationRetry();
}

void AGGJCharacterGroupManager::ScheduleInitializationRetry()
{
    if (!bInitialGroupCreated && !bEndingPlay && GetWorld())
    {
        GetWorldTimerManager().SetTimer(InitializationTimer, this,
            &AGGJCharacterGroupManager::TryInitializeGroup, 0.05f, false);
    }
}

void AGGJCharacterGroupManager::TryInitializeGroup()
{
    if (bInitialGroupCreated || bEndingPlay) { return; }

    APlayerController* PlayerController = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    AGGJPhysicalAnimationCharacter* PlayerCharacter = PlayerController
        ? Cast<AGGJPhysicalAnimationCharacter>(PlayerController->GetPawn()) : nullptr;
    if (!PlayerCharacter)
    {
        // GameMode 和 PlayerStart 的生成时序因 PIE/独立游戏而不同，安全地等下一次检查。
        ScheduleInitializationRetry();
        return;
    }

    RegisterMember(PlayerCharacter, true);
    if (!MemberClass)
    {
        MemberClass = PlayerCharacter->GetClass();
    }

    InitialPopulation = FMath::Clamp(InitialPopulation, 1, MaxPopulation);
    if (!CreateInitialPopulationFromSpawnZones(PlayerCharacter))
    {
        // 开场成员必须立即就绪；逐个掉落只用于游戏过程中主动调用 Add Population。
        const int32 FollowerCount = InitialPopulation - 1;
        for (int32 Index = 0; Index < FollowerCount; ++Index)
        {
            if (AGGJPhysicalAnimationCharacter* Character = SpawnMember(GetFormationLocation(
                Index, FollowerCount, PlayerCharacter->GetActorLocation())))
            {
                RegisterMember(Character, false);
            }
        }
    }
    bInitialGroupCreated = true;
    BroadcastPopulationChanged();
}

int32 AGGJCharacterGroupManager::GetPopulationCount() const
{
    int32 Count = 0;
    for (const TWeakObjectPtr<AGGJPhysicalAnimationCharacter>& Member : Members)
    {
        Count += Member.IsValid() ? 1 : 0;
    }
    return Count;
}

TArray<AGGJPhysicalAnimationCharacter*> AGGJCharacterGroupManager::GetMembers() const
{
    TArray<AGGJPhysicalAnimationCharacter*> Result;
    Result.Reserve(Members.Num());
    for (const TWeakObjectPtr<AGGJPhysicalAnimationCharacter>& Member : Members)
    {
        if (AGGJPhysicalAnimationCharacter* Character = Member.Get())
        {
            Result.Add(Character);
        }
    }
    return Result;
}

bool AGGJCharacterGroupManager::RegisterMember(AGGJPhysicalAnimationCharacter* Character,
    const bool bAsLeader)
{
    if (!IsValid(Character) || bEndingPlay) { return false; }
    RemoveInvalidMembers();

    const bool bAlreadyRegistered = Members.ContainsByPredicate(
        [Character](const TWeakObjectPtr<AGGJPhysicalAnimationCharacter>& Entry)
        {
            return Entry.Get() == Character;
        });
    if (!bAlreadyRegistered)
    {
        Members.Add(Character);
    }
    if (bAsLeader || !Leader.IsValid())
    {
        Leader = Character;
    }
    Character->SetCharacterGroupManager(this);

    if (!bAlreadyRegistered)
    {
        BroadcastPopulationChanged();
    }
    return !bAlreadyRegistered;
}

void AGGJCharacterGroupManager::UnregisterMember(AGGJPhysicalAnimationCharacter* Character)
{
    if (!Character) { return; }
    const int32 Removed = Members.RemoveAll(
        [Character](const TWeakObjectPtr<AGGJPhysicalAnimationCharacter>& Entry)
        {
            return !Entry.IsValid() || Entry.Get() == Character;
        });
    if (Leader.Get() == Character)
    {
        Leader.Reset();
    }
    if (Removed > 0 && !bEndingPlay)
    {
        BroadcastPopulationChanged();
    }
}

bool AGGJCharacterGroupManager::DestroyMember(AGGJPhysicalAnimationCharacter* Character)
{
    if (!IsValid(Character) || bEndingPlay) { return false; }
    RemoveInvalidMembers();
    const bool bWasMember = Members.ContainsByPredicate(
        [Character](const TWeakObjectPtr<AGGJPhysicalAnimationCharacter>& Entry)
        {
            return Entry.Get() == Character;
        });
    if (!bWasMember) { return false; }

    const bool bWasLeader = Leader.Get() == Character;
    Members.RemoveAll([Character](const TWeakObjectPtr<AGGJPhysicalAnimationCharacter>& Entry)
    {
        return !Entry.IsValid() || Entry.Get() == Character;
    });
    Character->SetCharacterGroupManager(nullptr);

    if (bWasLeader)
    {
        Leader.Reset();
        PromoteLeaderAfterRemoval(Character);
    }
    Character->Destroy();
    BroadcastPopulationChanged();
    return true;
}

void AGGJCharacterGroupManager::PromoteLeaderAfterRemoval(
    AGGJPhysicalAnimationCharacter* RemovedLeader)
{
    AGGJPhysicalAnimationCharacter* Replacement = nullptr;
    for (const TWeakObjectPtr<AGGJPhysicalAnimationCharacter>& Member : Members)
    {
        if (Member.IsValid())
        {
            Replacement = Member.Get();
            break;
        }
    }
    Leader = Replacement;

    APlayerController* Controller = RemovedLeader
        ? Cast<APlayerController>(RemovedLeader->GetController()) : nullptr;
    if (!Controller && GetWorld())
    {
        Controller = GetWorld()->GetFirstPlayerController();
    }
    if (Controller && Controller->GetPawn() == RemovedLeader)
    {
        Controller->UnPossess();
    }
    if (Controller && Replacement)
    {
        Controller->Possess(Replacement);
        if (GroupCamera.IsValid())
        {
            Controller->SetViewTarget(GroupCamera.Get());
        }
    }
}

int32 AGGJCharacterGroupManager::AddPopulation(const int32 Amount, const FVector SpawnCenter)
{
    if (Amount <= 0 || bEndingPlay || !GetWorld()) { return 0; }
    RemoveInvalidMembers();

    if (!MemberClass && Leader.IsValid())
    {
        MemberClass = Leader->GetClass();
    }
    if (!MemberClass) { return 0; }

    // 排队中的人物同样占用名额，防止短时间连续调用突破 MaxPopulation。
    const int32 AvailableSlots = FMath::Max(0,
        MaxPopulation - GetPopulationCount() - PendingSpawnLocations.Num());
    const int32 Requested = FMath::Min(Amount, AvailableSlots);
    for (int32 Index = 0; Index < Requested; ++Index)
    {
        PendingSpawnLocations.Add(GetFormationLocation(Index, Requested, SpawnCenter)
            + FVector::UpVector * FMath::Max(0.f, RuntimeSpawnDropHeight));
    }
    EnsurePopulationSpawnTimer();
    return Requested;
}

void AGGJCharacterGroupManager::EnsurePopulationSpawnTimer()
{
    if (bEndingPlay || bProcessingSpawnQueue || PendingSpawnLocations.IsEmpty() || !GetWorld())
    {
        return;
    }
    if (GetWorldTimerManager().IsTimerActive(PopulationSpawnTimer))
    {
        return;
    }

    // 第一人立刻出现，让按钮触发有即时反馈；剩余人物再按固定节奏依次掉落。
    ProcessNextPendingSpawn();
    if (!PendingSpawnLocations.IsEmpty())
    {
        GetWorldTimerManager().SetTimer(PopulationSpawnTimer, this,
            &AGGJCharacterGroupManager::ProcessNextPendingSpawn,
            FMath::Max(0.02f, RuntimeSpawnInterval), true);
    }
}

void AGGJCharacterGroupManager::ProcessNextPendingSpawn()
{
    if (bEndingPlay || !GetWorld())
    {
        CancelPendingPopulation();
        return;
    }
    if (PendingSpawnLocations.IsEmpty())
    {
        GetWorldTimerManager().ClearTimer(PopulationSpawnTimer);
        return;
    }

    bProcessingSpawnQueue = true;
    const FVector SpawnLocation = PendingSpawnLocations[0];
    PendingSpawnLocations.RemoveAt(0, 1, EAllowShrinking::No);
    if (AGGJPhysicalAnimationCharacter* Character = SpawnMember(SpawnLocation))
    {
        RegisterMember(Character, false);
    }
    bProcessingSpawnQueue = false;

    if (PendingSpawnLocations.IsEmpty())
    {
        GetWorldTimerManager().ClearTimer(PopulationSpawnTimer);
    }
}

void AGGJCharacterGroupManager::CancelPendingPopulation()
{
    PendingSpawnLocations.Reset();
    if (GetWorld())
    {
        GetWorldTimerManager().ClearTimer(PopulationSpawnTimer);
    }
}

int32 AGGJCharacterGroupManager::RemovePopulation(const int32 Amount)
{
    if (Amount <= 0 || bEndingPlay) { return 0; }
    RemoveInvalidMembers();

    // 先摘出弱引用再 Destroy，避免 EndPlay -> UnregisterMember 回调修改正在遍历的数组。
    TArray<AGGJPhysicalAnimationCharacter*> Victims;
    for (int32 Index = Members.Num() - 1; Index >= 0 && Victims.Num() < Amount; --Index)
    {
        AGGJPhysicalAnimationCharacter* Character = Members[Index].Get();
        if (Character && Character != Leader.Get())
        {
            Victims.Add(Character);
            Members.RemoveAt(Index);
        }
    }
    for (AGGJPhysicalAnimationCharacter* Victim : Victims)
    {
        Victim->SetCharacterGroupManager(nullptr);
        Victim->Destroy();
    }
    if (!Victims.IsEmpty())
    {
        BroadcastPopulationChanged();
    }
    return Victims.Num();
}

int32 AGGJCharacterGroupManager::RequestPopulationDelta(const int32 Delta,
    const FVector SpawnCenter)
{
    if (Delta > 0) { return AddPopulation(Delta, SpawnCenter); }
    if (Delta < 0) { return -RemovePopulation(-Delta); }
    return 0;
}

void AGGJCharacterGroupManager::ApplySharedMovement(FVector WorldMovement)
{
    if (WorldMovement.ContainsNaN()) { WorldMovement = FVector::ZeroVector; }
    WorldMovement.Z = 0.f;
    WorldMovement = WorldMovement.GetClampedToMaxSize(1.f);
    RemoveInvalidMembers();

    for (const TWeakObjectPtr<AGGJPhysicalAnimationCharacter>& Member : Members)
    {
        if (AGGJPhysicalAnimationCharacter* Character = Member.Get())
        {
            Character->ApplySharedWorldMovement(WorldMovement);
        }
    }
}

void AGGJCharacterGroupManager::ApplySharedJumpStart()
{
    RemoveInvalidMembers();
    for (const TWeakObjectPtr<AGGJPhysicalAnimationCharacter>& Member : Members)
    {
        if (AGGJPhysicalAnimationCharacter* Character = Member.Get())
        {
            Character->ApplySharedJumpStart();
        }
    }
}

void AGGJCharacterGroupManager::ApplySharedJumpEnd()
{
    RemoveInvalidMembers();
    for (const TWeakObjectPtr<AGGJPhysicalAnimationCharacter>& Member : Members)
    {
        if (AGGJPhysicalAnimationCharacter* Character = Member.Get())
        {
            Character->ApplySharedJumpEnd();
        }
    }
}

FVector AGGJCharacterGroupManager::GetFormationLocation(const int32 LocalIndex,
    const int32 BatchCount, const FVector& Center) const
{
    const AGGJPhysicalAnimationCharacter* Reference = Leader.Get();
    const FVector Right = Reference ? Reference->GetActorRightVector() : FVector::RightVector;
    const FVector Forward = Reference ? Reference->GetActorForwardVector() : FVector::ForwardVector;
    const FVector Offset = GGJSpawnPacking::MakeCompactStackOffset(LocalIndex, BatchCount,
        FormationSpacing, SpawnLayerCapacity, SpawnLayerHeight, SpawnPositionJitter);
    return Center + Forward * Offset.X + Right * Offset.Y + FVector::UpVector * Offset.Z;
}

AGGJPhysicalAnimationCharacter* AGGJCharacterGroupManager::SpawnMember(const FVector& Location)
{
    const FTransform SpawnTransform(Leader.IsValid() ? Leader->GetActorRotation() : FRotator::ZeroRotator,
        Location);
    AGGJPhysicalAnimationCharacter* Character =
        GetWorld()->SpawnActorDeferred<AGGJPhysicalAnimationCharacter>(MemberClass, SpawnTransform,
            this, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
    if (!Character) { return nullptr; }
    Character->FinishSpawning(SpawnTransform);
    return Character;
}

bool AGGJCharacterGroupManager::CreateInitialPopulationFromSpawnZones(
    AGGJPhysicalAnimationCharacter* PlayerCharacter)
{
    if (!GetWorld() || !PlayerCharacter || InitialPopulation <= 0) { return false; }

    TArray<AGGJSpawnZone*> SpawnZones;
    for (TActorIterator<AGGJSpawnZone> It(GetWorld()); It; ++It)
    {
        if (IsValid(*It) && It->IsSpawnEnabled())
        {
            SpawnZones.Add(*It);
        }
    }
    SpawnZones.Sort([](const AGGJSpawnZone& A, const AGGJSpawnZone& B)
    {
        if (A.SpawnOrder != B.SpawnOrder) { return A.SpawnOrder < B.SpawnOrder; }
        return A.GetFName().LexicalLess(B.GetFName());
    });
    if (SpawnZones.IsEmpty()) { return false; }

    TArray<FVector> SpawnLocations;
    SpawnLocations.Reserve(InitialPopulation);
    const int32 BaseCount = InitialPopulation / SpawnZones.Num();
    const int32 Remainder = InitialPopulation % SpawnZones.Num();
    for (int32 ZoneIndex = 0; ZoneIndex < SpawnZones.Num(); ++ZoneIndex)
    {
        const int32 ZoneCount = BaseCount + (ZoneIndex < Remainder ? 1 : 0);
        for (int32 LocalIndex = 0; LocalIndex < ZoneCount; ++LocalIndex)
        {
            SpawnLocations.Add(SpawnZones[ZoneIndex]->GetSpawnLocation(
                LocalIndex, ZoneCount, FormationSpacing, SpawnLayerCapacity,
                SpawnLayerHeight, SpawnPositionJitter));
        }
    }
    if (SpawnLocations.IsEmpty()) { return false; }

    PlayerCharacter->SetActorLocationAndRotation(SpawnLocations[0],
        SpawnZones[0]->GetActorRotation(), false, nullptr, ETeleportType::TeleportPhysics);
    for (int32 Index = 1; Index < SpawnLocations.Num(); ++Index)
    {
        if (AGGJPhysicalAnimationCharacter* Character = SpawnMember(SpawnLocations[Index]))
        {
            RegisterMember(Character, false);
        }
    }
    return true;
}

void AGGJCharacterGroupManager::RemoveInvalidMembers()
{
    Members.RemoveAll([](const TWeakObjectPtr<AGGJPhysicalAnimationCharacter>& Entry)
    {
        return !Entry.IsValid();
    });
}

void AGGJCharacterGroupManager::BroadcastPopulationChanged()
{
    OnPopulationChanged.Broadcast(GetPopulationCount());
}

void AGGJCharacterGroupManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    bEndingPlay = true;
    GetWorldTimerManager().ClearTimer(InitializationTimer);
    GetWorldTimerManager().ClearTimer(PopulationSpawnTimer);
    PendingSpawnLocations.Reset();
    for (const TWeakObjectPtr<AGGJPhysicalAnimationCharacter>& Member : Members)
    {
        if (AGGJPhysicalAnimationCharacter* Character = Member.Get())
        {
            Character->SetCharacterGroupManager(nullptr);
        }
    }
    Members.Reset();
    Leader.Reset();
    GroupCamera.Reset();
    Super::EndPlay(EndPlayReason);
}
