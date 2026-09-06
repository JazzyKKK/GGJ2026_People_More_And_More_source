#pragma once

// 可选的基础动态灯光组合。
// 它用于快速获得 3D 明暗和投影，不属于人群逻辑，也不会由 GameMode 自动生成。

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CrowdLightingRig.generated.h"

class UDirectionalLightComponent;
class USkyLightComponent;

/** 由关卡设计师按需放置的主方向光 + 天空补光。 */
UCLASS(Blueprintable)
class GGJ_GAMEDEMO_API ACrowdLightingRig : public AActor
{
    GENERATED_BODY()
public:
    /** 创建可移动的主光和环境补光默认值。 */
    ACrowdLightingRig();

    /** 产生明确方向、亮面和动态阴影的 Directional Light。 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Crowd|Lighting")
    TObjectPtr<UDirectionalLightComponent> KeyLight;

    /** 使用环境 Cubemap 填充背光面，防止角色暗部全黑。 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Crowd|Lighting")
    TObjectPtr<USkyLightComponent> FillLight;
};
