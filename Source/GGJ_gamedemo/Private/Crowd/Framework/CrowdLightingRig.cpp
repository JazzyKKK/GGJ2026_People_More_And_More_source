// 可选基础灯光 Actor 的组件默认值实现。
// 仅提供快速可编辑的 3D 光影，不参与游戏状态或关卡生成。
#include "Crowd/Framework/CrowdLightingRig.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/TextureCube.h"
#include "UObject/ConstructorHelpers.h"

ACrowdLightingRig::ACrowdLightingRig()
{
    // 灯光由渲染线程处理，本 Actor 不需要玩法 Tick。
    PrimaryActorTick.bCanEverTick = false;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
    KeyLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("KeyLight"));
    KeyLight->SetupAttachment(RootComponent);
    KeyLight->SetMobility(EComponentMobility::Movable);
    // 默认使用斜向暖光，让圆柱小人的体积和地面投影一眼可见。
    KeyLight->SetRelativeRotation(FRotator(-45.f, -35.f, 0.f));
    KeyLight->SetIntensity(5.f);
    KeyLight->SetCastShadows(true);
    KeyLight->SetLightColor(FLinearColor(1.f, 0.91f, 0.8f));
    FillLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("FillLight"));
    FillLight->SetupAttachment(RootComponent);
    FillLight->SetMobility(EComponentMobility::Movable);
    // 指定 Cubemap 可在没有完整天空系统的测试关卡中稳定提供环境光。
    FillLight->SourceType = ESkyLightSourceType::SLS_SpecifiedCubemap;
    static ConstructorHelpers::FObjectFinder<UTextureCube> SkyCube(
        TEXT("/Engine/MapTemplates/Sky/DaylightAmbientCubemap.DaylightAmbientCubemap"));
    FillLight->Cubemap = SkyCube.Object;
    FillLight->SetIntensity(0.4f);
}
