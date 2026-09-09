#include "Gameplay/Zones/GGJCountZone.h"

#include "Character/GGJCharacterGroupManager.h"
#include "Character/GGJPhysicalAnimationCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Gameplay/Effects/GGJSquareZoneVFX.h"
#include "Gameplay/Zones/GGJZoneDetection.h"

AGGJCountZone::AGGJCountZone()
{
    PrimaryActorTick.bCanEverTick = true;

    CountBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("CountBounds"));
    SetRootComponent(CountBounds);
    CountBounds->SetBoxExtent(FVector(350.f, 350.f, 160.f));
    CountBounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    CountBounds->SetCollisionObjectType(ECC_WorldDynamic);
    CountBounds->SetCollisionResponseToAllChannels(ECR_Ignore);
    CountBounds->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    CountBounds->SetGenerateOverlapEvents(true);
    CountBounds->SetHiddenInGame(true);
    CountBounds->ShapeColor = FColor(245, 185, 35);

    StatusText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("StatusText"));
    StatusText->SetupAttachment(CountBounds);
    StatusText->SetHorizontalAlignment(EHTA_Center);
    StatusText->SetVerticalAlignment(EVRTA_TextCenter);
    StatusText->SetWorldSize(TextWorldSize);
    StatusText->SetText(FText::FromString(TEXT("?")));
    StatusText->SetTextRenderColor(WaitingColor.ToFColor(true));
    StatusText->SetCastShadow(false);

    CustomFloatingText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("CustomFloatingText"));
    CustomFloatingText->SetupAttachment(CountBounds);
    CustomFloatingText->SetHorizontalAlignment(EHTA_Center);
    CustomFloatingText->SetVerticalAlignment(EVRTA_TextCenter);
    CustomFloatingText->SetWorldSize(CustomFloatingTextWorldSize);
    CustomFloatingText->SetText(CustomFloatingTextContent);
    CustomFloatingText->SetTextRenderColor(CustomFloatingTextColor.ToFColor(true));
    CustomFloatingText->SetCastShadow(false);
    CustomFloatingText->SetVisibility(false);

    // 独立的轻量渲染组件：无碰撞，不会改变 CountBounds 的检测结果。
    SquareZoneVFX = CreateDefaultSubobject<UGGJSquareZoneVFXComponent>(TEXT("SquareZoneVFX"));
    SquareZoneVFX->SetupAttachment(CountBounds);
}

void AGGJCountZone::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    if (StatusText)
    {
        StatusText->SetRelativeLocation(FVector(0.f, 0.f, TextHeight));
        StatusText->SetWorldSize(FMath::Max(10.f, TextWorldSize));
        StatusText->SetText(FText::FromString(TEXT("?")));
        StatusText->SetTextRenderColor(WaitingColor.ToFColor(true));
    }
    if (CustomFloatingText)
    {
        CustomFloatingText->SetRelativeLocation(CustomFloatingTextOffset);
        CustomFloatingText->SetWorldSize(FMath::Max(10.f, CustomFloatingTextWorldSize));
        CustomFloatingText->SetText(CustomFloatingTextContent);
        CustomFloatingText->SetTextRenderColor(CustomFloatingTextColor.ToFColor(true));
        CustomFloatingText->SetVisibility(bShowCustomFloatingText);
    }
    SynchronizeSquareZoneVFX(true);
}

void AGGJCountZone::BeginPlay()
{
    Super::BeginPlay();
    ResolveManager();
    RefreshCountState();
    SynchronizeSquareZoneVFX(true);
}

void AGGJCountZone::Tick(const float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    RefreshCountState();
    UpdateVisual(DeltaSeconds);
    SynchronizeSquareZoneVFX();
}

void AGGJCountZone::RefreshSquareZoneVFX()
{
    SynchronizeSquareZoneVFX(true);
}

void AGGJCountZone::SynchronizeSquareZoneVFX(const bool bForceRefresh)
{
    if (!SquareZoneVFX || !CountBounds)
    {
        return;
    }

    const FVector BoxExtent = CountBounds->GetUnscaledBoxExtent();
    const FVector2D NewExtent(
        FMath::Max(10.f, FMath::Abs(BoxExtent.X)),
        FMath::Max(10.f, FMath::Abs(BoxExtent.Y)));
    const FLinearColor NewColor = bIsTriggered
        ? SquareSuccessColor
        : (InsidePopulation > 0 ? SquareActiveColor : SquareWaitingColor);
    const bool bNeedsRefresh = bForceRefresh
        || !NewExtent.Equals(LastSquareVFXExtent, 0.1f)
        || !NewColor.Equals(LastSquareVFXColor, 0.001f)
        || bLastSquareVFXVisible != bShowSquareZoneVFX;

    SquareZoneVFX->SetVisibility(bShowSquareZoneVFX, true);
    SquareZoneVFX->SetRelativeLocation(FVector(0.f, 0.f, SquareVFXHeightOffset));
    if (!bNeedsRefresh)
    {
        return;
    }

    SquareZoneVFX->ZoneExtent = NewExtent;
    SquareZoneVFX->RiseHeight = FMath::Max(10.f, SquareParticleRiseHeight);
    SquareZoneVFX->RiseSpeed = FMath::Max(1.f, SquareParticleRiseSpeed);
    SquareZoneVFX->OrbitSpeed = SquareParticleOrbitSpeed;
    SquareZoneVFX->bUseWorldSpaceParticleSize = bUseWorldSpaceSquareParticleSize;
    SquareZoneVFX->WorldParticleSize = FMath::Max(0.5f, SquareParticleWorldSize);
    SquareZoneVFX->PixelSize = FMath::Clamp(SquareParticleSize, 1.f, 40.f);
    SquareZoneVFX->BoundaryThickness = FMath::Clamp(SquareBoundaryThickness, 0.5f, 24.f);
    SquareZoneVFX->ParticleCount = FMath::Clamp(SquareParticleCount, 4, 512);
    SquareZoneVFX->LargeParticleRatio = FMath::Clamp(SquareLargeParticleRatio, 0.f, 1.f);
    SquareZoneVFX->RandomSeed = SquareParticleRandomSeed;
    SquareZoneVFX->bDrawBoundary = bShowSquareBoundary;
    SquareZoneVFX->PrimaryColor = NewColor;
    SquareZoneVFX->SecondaryColor = FMath::Lerp(NewColor, FLinearColor::White, 0.52f);
    SquareZoneVFX->UpdateBounds();
    SquareZoneVFX->MarkRenderStateDirty();

    LastSquareVFXExtent = NewExtent;
    LastSquareVFXColor = NewColor;
    bLastSquareVFXVisible = bShowSquareZoneVFX;
}

AGGJCharacterGroupManager* AGGJCountZone::ResolveManager()
{
    if (GroupManager.IsValid()) { return GroupManager.Get(); }
    if (!GetWorld()) { return nullptr; }
    for (TActorIterator<AGGJCharacterGroupManager> It(GetWorld()); It; ++It)
    {
        GroupManager = *It;
        return *It;
    }
    return nullptr;
}

void AGGJCountZone::RefreshCountState()
{
    AGGJCharacterGroupManager* Manager = ResolveManager();
    const int32 NewScenePopulation = Manager ? Manager->GetPopulationCount() : 0;

    int32 NewInsidePopulation = 0;
    if (Manager)
    {
        // 直接遍历 Manager 名册并做 Box 局部空间判断；不受角色碰撞预设和骨骼模拟影响。
        for (const AGGJPhysicalAnimationCharacter* Character : Manager->GetMembers())
        {
            if (GGJZoneDetection::IsActorOriginInsideBox(CountBounds, Character, DetectionPadding))
            {
                ++NewInsidePopulation;
            }
        }
    }
    const bool bCountsChanged = NewInsidePopulation != InsidePopulation
        || NewScenePopulation != ScenePopulation;
    InsidePopulation = NewInsidePopulation;
    ScenePopulation = NewScenePopulation;

    const bool bNewTriggered = bEnabled
        && EvaluateCountCondition(InsidePopulation, ScenePopulation, FMath::Max(0, TargetCount));
    if (bCountsChanged)
    {
        OnCountUpdated.Broadcast(InsidePopulation, ScenePopulation);
    }
    if (bNewTriggered != bIsTriggered)
    {
        bIsTriggered = bNewTriggered;
        VisualTime = 0.f;
        OnTriggeredChanged.Broadcast(bIsTriggered);
    }
}

bool AGGJCountZone::EvaluateCountCondition_Implementation(const int32 InsideCount,
    const int32 TotalCount, const int32 Target) const
{
    switch (Comparison)
    {
    case EGGJCountComparison::Equal: return InsideCount == Target;
    case EGGJCountComparison::NotEqual: return InsideCount != Target;
    case EGGJCountComparison::Greater: return InsideCount > Target;
    case EGGJCountComparison::GreaterOrEqual: return InsideCount >= Target;
    case EGGJCountComparison::Less: return InsideCount < Target;
    case EGGJCountComparison::LessOrEqual: return InsideCount <= Target;
    case EGGJCountComparison::Custom:
    default:
        // Custom 默认不误触发；蓝图覆写本函数后自行返回结果。
        return false;
    }
}

FText AGGJCountZone::GetComparisonLabel() const
{
    switch (Comparison)
    {
    case EGGJCountComparison::Equal: return FText::FromString(TEXT("="));
    case EGGJCountComparison::NotEqual: return FText::FromString(TEXT("!="));
    case EGGJCountComparison::Greater: return FText::FromString(TEXT(">"));
    case EGGJCountComparison::GreaterOrEqual: return FText::FromString(TEXT(">="));
    case EGGJCountComparison::Less: return FText::FromString(TEXT("<"));
    case EGGJCountComparison::LessOrEqual: return FText::FromString(TEXT("<="));
    case EGGJCountComparison::Custom:
    default: return CustomComparisonLabel;
    }
}

void AGGJCountZone::UpdateVisual(const float DeltaSeconds)
{
    VisualTime += FMath::Max(0.f, DeltaSeconds);
    if (StatusText)
    {
        StatusText->SetRelativeLocation(FVector(0.f, 0.f, TextHeight));
        StatusText->SetWorldSize(FMath::Max(10.f, TextWorldSize));

        if (InsidePopulation <= 0)
        {
            StatusText->SetText(FText::FromString(TEXT("?")));
            StatusText->SetTextRenderColor(WaitingColor.ToFColor(true));
        }
        else
        {
            const FString Display = FString::Printf(TEXT("%d%s%d"), InsidePopulation,
                *GetComparisonLabel().ToString(), FMath::Max(0, TargetCount));
            StatusText->SetText(FText::FromString(Display));
            StatusText->SetTextRenderColor(
                (bIsTriggered ? SuccessColor : ActiveColor).ToFColor(true));
        }

        const float Pulse = bIsTriggered
            ? 1.f + FMath::Sin(VisualTime * FMath::Max(0.f, SuccessPulseSpeed))
                * FMath::Max(0.f, SuccessPulseAmplitude)
            : 1.f;
        StatusText->SetRelativeScale3D(FVector(Pulse));
    }

    if (CustomFloatingText)
    {
        CustomFloatingText->SetVisibility(bShowCustomFloatingText);
        if (bShowCustomFloatingText)
        {
            CustomFloatingText->SetRelativeLocation(CustomFloatingTextOffset);
            CustomFloatingText->SetWorldSize(FMath::Max(10.f, CustomFloatingTextWorldSize));
            CustomFloatingText->SetText(CustomFloatingTextContent);
            CustomFloatingText->SetTextRenderColor(CustomFloatingTextColor.ToFColor(true));
            CustomFloatingText->SetRelativeScale3D(FVector::OneVector);
        }
    }

    if (GetWorld())
    {
        if (const APlayerController* Controller = GetWorld()->GetFirstPlayerController())
        {
            FVector ViewLocation;
            FRotator ViewRotation;
            Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
            if (StatusText && bFacePlayerCamera)
            {
                StatusText->SetWorldRotation(
                    (ViewLocation - StatusText->GetComponentLocation()).Rotation());
            }
            if (CustomFloatingText && bShowCustomFloatingText && bCustomTextFacesPlayerCamera)
            {
                CustomFloatingText->SetWorldRotation(
                    (ViewLocation - CustomFloatingText->GetComponentLocation()).Rotation());
            }
        }
    }
}
