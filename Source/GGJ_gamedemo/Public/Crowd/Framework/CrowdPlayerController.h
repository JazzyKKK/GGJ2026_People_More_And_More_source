#pragma once

// 玩家输入总入口。
// Controller 不直接移动某个 Pawn，而是把一份镜头相对输入发布给整个人群。

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "CrowdPlayerController.generated.h"

class UInputAction;
class UInputMappingContext;

/**
 * 读取 Enhanced Input、控制轨道相机，并向 PopulationSubsystem 发布共享世界方向。
 * 蓝图可提供正式 Input Assets；若全部留空，C++ 会在运行时创建可玩的兜底映射。
 */
UCLASS(Blueprintable)
class GGJ_GAMEDEMO_API ACrowdPlayerController : public APlayerController
{
    GENERATED_BODY()
public:
    /** 添加输入 Mapping Context，并切到纯游戏输入模式。 */
    virtual void BeginPlay() override;

    /** 创建/检查输入资产，并把 Action 事件绑定到处理函数。 */
    virtual void SetupInputComponent() override;

    /** 清零共享输入并移除本 Controller 添加的 Mapping Context。 */
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    /** Action 计算完后推进相机，再把屏幕输入转换为世界输入；每帧只发布一次。 */
    virtual void PostProcessInput(const float DeltaTime, const bool bGamePaused) override;

    // 这组资产是可选的完整配置；全部不填时，WASD/方向键/手柄/Q/E/R 仍可直接工作。
    /** 正式输入映射；若配置它，应同时配置 Move 和 Restart Action。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Crowd|Input")
    TObjectPtr<UInputMappingContext> InputMapping;

    /** Axis2D 移动 Action：X=屏幕左右，Y=屏幕上下。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Crowd|Input")
    TObjectPtr<UInputAction> MoveAction;

    /** Bool 重置 Action，默认键 R。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Crowd|Input")
    TObjectPtr<UInputAction> RestartAction;

    /** Bool 左转 Action，默认键 Q。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Crowd|Input")
    TObjectPtr<UInputAction> RotateLeftAction;

    /** Bool 右转 Action，默认键 E。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Crowd|Input")
    TObjectPtr<UInputAction> RotateRightAction;

private:
    /** 补齐缺少的 Action/Context；不覆盖用户已经配置的完整移动映射。 */
    void EnsureInputAssets();

    /** 缓存本帧 Axis2D 值，等待 PostProcessInput 统一发布。 */
    void HandleMove(const FInputActionValue& Value);

    /** Action Completed/Canceled 时明确清零缓存。 */
    void ClearMove(const FInputActionValue& Value);

    /** 找到本关唯一 Director 并重建初始人群。 */
    void Restart();

    /** 请求相机向左/右转 90°。 */
    void RotateLeft();
    void RotateRight();

    /** 仅当 Q/E Action 缺失时创建的独立兜底 Context，优先级高于主 Context。 */
    UPROPERTY(Transient)
    TObjectPtr<UInputMappingContext> OrbitMapping;

    /** 本帧屏幕空间输入；每次 PostProcessInput 结尾都会归零，防止焦点丢失后卡住。 */
    FVector2D CurrentScreenInput = FVector2D::ZeroVector;
};
