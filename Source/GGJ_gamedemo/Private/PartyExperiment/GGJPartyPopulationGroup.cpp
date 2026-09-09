#include "PartyExperiment/GGJPartyPopulationGroup.h"

#include "Character/GGJPhysicalAnimationCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Gameplay/Zones/GGJSpawnPacking.h"
#include "PartyExperiment/Zones/GGJPartySpawnZone.h"

AGGJPartyPopulationGroup::AGGJPartyPopulationGroup()
{
    PrimaryActorTick.bCanEverTick = true;
    // PlayerController 在一帧中写入控制方向，人口组下一帧统一求一次推挤，避免按成员重复 Tick。
    PrimaryActorTick.TickGroup = TG_PrePhysics;
}

void AGGJPartyPopulationGroup::Tick(const float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (bEnableCharacterPushing && !LastWorldMovement.IsNearlyZero()
        && FMath::IsFinite(DeltaSeconds) && DeltaSeconds > 0.f)
    {
        ApplyCharacterPushing();
    }
}

void AGGJPartyPopulationGroup::InitializeGroup(const FGGJPartyPlayerSetup& Setup,
    const TArray<AGGJPartySpawnZone*>& SpawnZones,
    TSubclassOf<AGGJPhysicalAnimationCharacter> FallbackMemberClass)
{
    if (bInitialized || bEndingPlay || !GetWorld())
    {
        return;
    }

    PlayerIndex = FMath::Max(0, Setup.PlayerIndex);
    MaxPopulation = FMath::Clamp(Setup.MaxPopulation, 1, 500);
    InitialPopulation = FMath::Clamp(Setup.InitialPopulation, 0, MaxPopulation);
    MemberClass = Setup.MemberClass ? Setup.MemberClass : FallbackMemberClass;
    if (!MemberClass)
    {
        MemberClass = AGGJPhysicalAnimationCharacter::StaticClass();
    }
    OcclusionStencilValue = FMath::Clamp(Setup.OcclusionStencilValue, 1, 255);

    TArray<AGGJPartySpawnZone*> ValidZones;
    for (AGGJPartySpawnZone* Zone : SpawnZones)
    {
        if (IsValid(Zone) && Zone->IsSpawnEnabled() && Zone->PlayerIndex == PlayerIndex)
        {
            ValidZones.Add(Zone);
        }
    }
    ValidZones.Sort([](const AGGJPartySpawnZone& A, const AGGJPartySpawnZone& B)
    {
        if (A.SpawnOrder != B.SpawnOrder)
        {
            return A.SpawnOrder < B.SpawnOrder;
        }
        return A.GetFName().LexicalLess(B.GetFName());
    });

    TArray<FTransform> InitialTransforms;
    InitialTransforms.Reserve(InitialPopulation);
    if (!ValidZones.IsEmpty())
    {
        // 同一名玩家拥有多个出生区时自动平分；余数优先分给 SpawnOrder 较小的区域。
        const int32 BaseCount = InitialPopulation / ValidZones.Num();
        const int32 Remainder = InitialPopulation % ValidZones.Num();
        for (int32 ZoneIndex = 0; ZoneIndex < ValidZones.Num(); ++ZoneIndex)
        {
            AGGJPartySpawnZone* Zone = ValidZones[ZoneIndex];
            const int32 ZoneCount = BaseCount + (ZoneIndex < Remainder ? 1 : 0);
            for (int32 LocalIndex = 0; LocalIndex < ZoneCount; ++LocalIndex)
            {
                InitialTransforms.Emplace(Zone->GetActorRotation(), Zone->GetSpawnLocation(
                    LocalIndex, ZoneCount, FormationSpacing, SpawnLayerCapacity,
                    SpawnLayerHeight, SpawnPositionJitter));
            }
        }
    }
    else
    {
        // 忘记放 PartySpawnZone 时仍然能进入实验；不同玩家的 Manager 应由 GameMode 分开放置。
        for (int32 Index = 0; Index < InitialPopulation; ++Index)
        {
            InitialTransforms.Emplace(GetActorRotation(),
                MakePackedLocation(Index, InitialPopulation, GetActorLocation()));
        }
        UE_LOG(LogTemp, Warning,
            TEXT("Party player %d has no matching PartySpawnZone; using manager location."),
            PlayerIndex + 1);
    }

    // 开局人物立即存在，避免输入开始时仍没有可控目标；逐个掉落只用于中途增长。
    for (const FTransform& SpawnTransform : InitialTransforms)
    {
        SpawnMember(SpawnTransform);
    }
    bInitialized = true;
    BroadcastPopulationChanged();
}

int32 AGGJPartyPopulationGroup::GetPopulationCount() const
{
    int32 Count = 0;
    for (const TWeakObjectPtr<AGGJPhysicalAnimationCharacter>& Member : Members)
    {
        Count += Member.IsValid() ? 1 : 0;
    }
    return Count;
}

TArray<AGGJPhysicalAnimationCharacter*> AGGJPartyPopulationGroup::GetMembers() const
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

bool AGGJPartyPopulationGroup::ContainsMember(
    const AGGJPhysicalAnimationCharacter* Character) const
{
    if (!IsValid(Character))
    {
        return false;
    }
    return Members.ContainsByPredicate(
        [Character](const TWeakObjectPtr<AGGJPhysicalAnimationCharacter>& Entry)
        {
            return Entry.Get() == Character;
        });
}

int32 AGGJPartyPopulationGroup::AddPopulation(const int32 Amount,
    const FVector SpawnCenter)
{
    if (Amount <= 0)
    {
        return 0;
    }

    TArray<FTransform> CandidateTransforms;
    CandidateTransforms.Reserve(Amount);
    for (int32 Index = 0; Index < Amount; ++Index)
    {
        CandidateTransforms.Emplace(GetActorRotation(),
            MakePackedLocation(Index, Amount, SpawnCenter)
                + FVector::UpVector * FMath::Max(0.f, RuntimeSpawnDropHeight));
    }
    return QueuePopulation(Amount, CandidateTransforms);
}

int32 AGGJPartyPopulationGroup::AddPopulationAtSpawnZone(const int32 Amount,
    AGGJPartySpawnZone* SpawnZone)
{
    if (Amount <= 0 || !IsValid(SpawnZone) || !SpawnZone->IsSpawnEnabled())
    {
        return 0;
    }
    if (SpawnZone->PlayerIndex != PlayerIndex)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("Party player %d rejected spawn zone %s owned by player %d."),
            PlayerIndex + 1, *SpawnZone->GetName(), SpawnZone->PlayerIndex + 1);
        return 0;
    }

    TArray<FTransform> CandidateTransforms;
    CandidateTransforms.Reserve(Amount);
    for (int32 Index = 0; Index < Amount; ++Index)
    {
        const FVector Location = SpawnZone->GetSpawnLocation(Index, Amount, FormationSpacing,
            SpawnLayerCapacity, SpawnLayerHeight, SpawnPositionJitter)
            + FVector::UpVector * FMath::Max(0.f, RuntimeSpawnDropHeight);
        CandidateTransforms.Emplace(SpawnZone->GetActorRotation(), Location);
    }
    return QueuePopulation(Amount, CandidateTransforms);
}

int32 AGGJPartyPopulationGroup::RequestPopulationDelta(const int32 Delta,
    const FVector SpawnCenter)
{
    if (Delta > 0)
    {
        return AddPopulation(Delta, SpawnCenter);
    }
    if (Delta < 0)
    {
        return -RemovePopulation(-Delta);
    }
    return 0;
}

int32 AGGJPartyPopulationGroup::QueuePopulation(const int32 Amount,
    const TArray<FTransform>& CandidateTransforms)
{
    if (Amount <= 0 || bEndingPlay || !bInitialized || !GetWorld() || !MemberClass)
    {
        return 0;
    }
    RemoveInvalidMembers();

    // 排队成员也占上限，避免蓝图在短时间内连续调用增长而突破 MaxPopulation。
    const int32 AvailableSlots = FMath::Max(0,
        MaxPopulation - GetPopulationCount() - PendingSpawnTransforms.Num());
    const int32 Accepted = FMath::Min3(Amount, CandidateTransforms.Num(), AvailableSlots);
    for (int32 Index = 0; Index < Accepted; ++Index)
    {
        PendingSpawnTransforms.Add(CandidateTransforms[Index]);
    }
    StartPendingSpawnTimerIfNeeded();
    return Accepted;
}

void AGGJPartyPopulationGroup::StartPendingSpawnTimerIfNeeded()
{
    if (bEndingPlay || bProcessingSpawnQueue || PendingSpawnTransforms.IsEmpty() || !GetWorld()
        || GetWorldTimerManager().IsTimerActive(PopulationSpawnTimer))
    {
        return;
    }

    // 第一人立即出现，余下成员按间隔逐个掉落。
    ProcessNextPendingSpawn();
    if (!PendingSpawnTransforms.IsEmpty())
    {
        GetWorldTimerManager().SetTimer(PopulationSpawnTimer, this,
            &AGGJPartyPopulationGroup::ProcessNextPendingSpawn,
            FMath::Max(0.02f, RuntimeSpawnInterval), true);
    }
}

void AGGJPartyPopulationGroup::ProcessNextPendingSpawn()
{
    if (bEndingPlay || !GetWorld())
    {
        CancelPendingPopulation();
        return;
    }
    if (PendingSpawnTransforms.IsEmpty())
    {
        GetWorldTimerManager().ClearTimer(PopulationSpawnTimer);
        return;
    }

    bProcessingSpawnQueue = true;
    const FTransform SpawnTransform = PendingSpawnTransforms[0];
    PendingSpawnTransforms.RemoveAt(0, 1, EAllowShrinking::No);
    SpawnMember(SpawnTransform);
    bProcessingSpawnQueue = false;

    if (PendingSpawnTransforms.IsEmpty())
    {
        GetWorldTimerManager().ClearTimer(PopulationSpawnTimer);
    }
}

AGGJPhysicalAnimationCharacter* AGGJPartyPopulationGroup::SpawnMember(
    const FTransform& SpawnTransform)
{
    if (!GetWorld() || !MemberClass)
    {
        return nullptr;
    }

    AGGJPhysicalAnimationCharacter* Character =
        GetWorld()->SpawnActorDeferred<AGGJPhysicalAnimationCharacter>(MemberClass,
            SpawnTransform, this, nullptr,
            ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
    if (!Character)
    {
        return nullptr;
    }

    // 角色不被任何 PlayerController 占有，由本 Group 直接广播世界方向。
    Character->SetCharacterGroupManager(nullptr);
    Character->SetOcclusionOutlineStencilValue(OcclusionStencilValue);
    Character->Tags.AddUnique(FName(*FString::Printf(TEXT("PartyPlayer_%d"), PlayerIndex)));
    Character->FinishSpawning(SpawnTransform);
    Members.Add(Character);

    OnMemberSpawned.Broadcast(PlayerIndex, Character);
    BroadcastPopulationChanged();
    return Character;
}

int32 AGGJPartyPopulationGroup::RemovePopulation(const int32 Amount)
{
    if (Amount <= 0 || bEndingPlay)
    {
        return 0;
    }
    RemoveInvalidMembers();

    TArray<AGGJPhysicalAnimationCharacter*> Victims;
    for (int32 Index = Members.Num() - 1; Index >= 0 && Victims.Num() < Amount; --Index)
    {
        if (AGGJPhysicalAnimationCharacter* Character = Members[Index].Get())
        {
            Victims.Add(Character);
        }
        Members.RemoveAt(Index);
    }
    for (AGGJPhysicalAnimationCharacter* Victim : Victims)
    {
        Victim->Destroy();
    }
    if (!Victims.IsEmpty())
    {
        BroadcastPopulationChanged();
    }
    return Victims.Num();
}

bool AGGJPartyPopulationGroup::DestroyMember(AGGJPhysicalAnimationCharacter* Character)
{
    if (!IsValid(Character) || bEndingPlay)
    {
        return false;
    }
    const int32 Removed = Members.RemoveAll(
        [Character](const TWeakObjectPtr<AGGJPhysicalAnimationCharacter>& Entry)
        {
            return !Entry.IsValid() || Entry.Get() == Character;
        });
    if (Removed <= 0)
    {
        return false;
    }
    Character->Destroy();
    BroadcastPopulationChanged();
    return true;
}

void AGGJPartyPopulationGroup::CancelPendingPopulation()
{
    PendingSpawnTransforms.Reset();
    if (GetWorld())
    {
        GetWorldTimerManager().ClearTimer(PopulationSpawnTimer);
    }
}

void AGGJPartyPopulationGroup::ApplySharedMovement(const FVector& WorldMovement)
{
    FVector SafeMovement = WorldMovement;
    if (SafeMovement.ContainsNaN())
    {
        SafeMovement = FVector::ZeroVector;
    }
    SafeMovement.Z = 0.f;
    SafeMovement = SafeMovement.GetClampedToMaxSize(1.f);
    LastWorldMovement = SafeMovement;
    RemoveInvalidMembers();

    for (const TWeakObjectPtr<AGGJPhysicalAnimationCharacter>& Member : Members)
    {
        if (AGGJPhysicalAnimationCharacter* Character = Member.Get())
        {
            Character->ApplySharedWorldMovement(SafeMovement);
        }
    }
}

void AGGJPartyPopulationGroup::ApplyCharacterPushing()
{
    if (!GetWorld())
    {
        return;
    }

    RemoveInvalidMembers();
    const FVector InputDirection = LastWorldMovement.GetSafeNormal2D();
    const float InputStrength = FMath::Clamp(LastWorldMovement.Size2D(), 0.f, 1.f);
    if (InputDirection.IsNearlyZero() || InputStrength <= UE_KINDA_SMALL_NUMBER)
    {
        return;
    }

    // 只收集 Owner 是 PartyPopulationGroup 的人物，避免实验模式误推关卡里其他 Character。
    TArray<AGGJPhysicalAnimationCharacter*> PartyCharacters;
    for (TActorIterator<AGGJPhysicalAnimationCharacter> It(GetWorld()); It; ++It)
    {
        if (IsValid(*It) && Cast<AGGJPartyPopulationGroup>(It->GetOwner()))
        {
            PartyCharacters.Add(*It);
        }
    }

    for (const TWeakObjectPtr<AGGJPhysicalAnimationCharacter>& SourceEntry : Members)
    {
        AGGJPhysicalAnimationCharacter* Source = SourceEntry.Get();
        const UCapsuleComponent* SourceCapsule = Source ? Source->GetCapsuleComponent() : nullptr;
        if (!Source || !SourceCapsule)
        {
            continue;
        }

        for (AGGJPhysicalAnimationCharacter* Target : PartyCharacters)
        {
            if (!IsValid(Target) || Target == Source)
            {
                continue;
            }
            const AGGJPartyPopulationGroup* TargetGroup =
                Cast<AGGJPartyPopulationGroup>(Target->GetOwner());
            const bool bSameGroup = TargetGroup == this;
            if ((bSameGroup && !bPushOwnGroup) || (!bSameGroup && !bPushOtherPlayers))
            {
                continue;
            }

            FVector ToTarget = Target->GetActorLocation() - Source->GetActorLocation();
            if (FMath::Abs(ToTarget.Z) > FMath::Max(0.f, CharacterPushVerticalTolerance))
            {
                continue;
            }
            ToTarget.Z = 0.f;

            const UCapsuleComponent* TargetCapsule = Target->GetCapsuleComponent();
            const float ContactDistance = SourceCapsule->GetScaledCapsuleRadius()
                + (TargetCapsule ? TargetCapsule->GetScaledCapsuleRadius() : 42.f)
                + FMath::Max(0.f, CharacterPushPadding);
            const float Distance = ToTarget.Size2D();
            if (Distance > ContactDistance)
            {
                continue;
            }

            const FVector TowardTarget = Distance > UE_KINDA_SMALL_NUMBER
                ? ToTarget / Distance : InputDirection;
            if (FVector::DotProduct(InputDirection, TowardTarget)
                < FMath::Clamp(CharacterPushMinForwardDot, -1.f, 1.f))
            {
                continue;
            }

            UCharacterMovementComponent* TargetMovement = Target->GetCharacterMovement();
            if (!TargetMovement)
            {
                continue;
            }

            // 越接近胶囊接触面，推力越完整。方向混合输入与双方连线，既能向前推也能自然侧滑。
            const float ProximityAlpha = 1.f - FMath::Clamp(
                Distance / FMath::Max(ContactDistance, 1.f), 0.f, 1.f);
            const float ForceScale = FMath::Lerp(0.35f, 1.f, ProximityAlpha) * InputStrength;
            const FVector PushDirection = (InputDirection + TowardTarget * 0.65f).GetSafeNormal2D();
            TargetMovement->AddForce(PushDirection
                * FMath::Max(0.f, CharacterPushForce) * ForceScale);
            ClampPushedCharacterSpeed(Target);
        }
    }
}

void AGGJPartyPopulationGroup::ClampPushedCharacterSpeed(
    AGGJPhysicalAnimationCharacter* Character) const
{
    UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
    if (!Movement)
    {
        return;
    }

    const float MaxSpeed = FMath::Max(50.f, CharacterPushMaxHorizontalSpeed);
    FVector Velocity = Movement->Velocity;
    const FVector2D Horizontal(Velocity.X, Velocity.Y);
    const FVector2D Clamped = Horizontal.GetClampedToMaxSize(MaxSpeed);
    Velocity.X = Clamped.X;
    Velocity.Y = Clamped.Y;
    Movement->Velocity = Velocity;
}

FVector AGGJPartyPopulationGroup::MakePackedLocation(const int32 LocalIndex,
    const int32 BatchCount, const FVector& Center) const
{
    const FVector Offset = GGJSpawnPacking::MakeCompactStackOffset(LocalIndex, BatchCount,
        FormationSpacing, SpawnLayerCapacity, SpawnLayerHeight, SpawnPositionJitter);
    const FRotator Rotation(0.f, GetActorRotation().Yaw, 0.f);
    return Center + Rotation.RotateVector(Offset);
}

void AGGJPartyPopulationGroup::RemoveInvalidMembers()
{
    Members.RemoveAll([](const TWeakObjectPtr<AGGJPhysicalAnimationCharacter>& Entry)
    {
        return !Entry.IsValid();
    });
}

void AGGJPartyPopulationGroup::BroadcastPopulationChanged()
{
    OnPopulationChanged.Broadcast(PlayerIndex, GetPopulationCount());
}

void AGGJPartyPopulationGroup::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    bEndingPlay = true;
    LastWorldMovement = FVector::ZeroVector;
    CancelPendingPopulation();
    Members.Reset();
    Super::EndPlay(EndPlayReason);
}
