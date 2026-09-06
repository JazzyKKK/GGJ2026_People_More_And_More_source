#pragma once

// 场地轨道相机 Pawn。
// Pawn 本身位于观察中心，Camera 组件以“中心的反方向 × 距离”放到轨道上。

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Crowd/Data/CrowdCameraSettings.h"
#include "CrowdCameraPawn.generated.h"

class UCameraComponent;

/** 一次 90° 转镜开始/结束时广播的无参数蓝图事件。 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCrowdViewRotationEvent);

/**
 * 玩家实际占有的唯一 Pawn，负责斜俯视透视镜头和 Q/E 四面观察。
 * 旋转不是 Actor Tick 驱动，而是由 PlayerController 在输入处理完成后每帧调用 AdvanceOrbit，
 * 这样同一帧发布的人群方向可以严格使用更新后的相机朝向。
 */
UCLASS(Blueprintable)
class GGJ_GAMEDEMO_API ACrowdCameraPawn : public APawn
{
    GENERATED_BODY()

public:
    /** 创建透视 Camera 组件并设置稳定曝光/干净的后处理默认值。 */
    ACrowdCameraPawn();

    /** 蓝图默认值变化时重置预览姿态。 */
    virtual void OnConstruction(const FTransform& Transform) override;

    /** 游戏开始时再次净化配置并回到初始观察角。 */
    virtual void BeginPlay() override;

    /** 把 Pawn 移到新的场地中心，并重置为初始角度。 */
    UFUNCTION(BlueprintCallable, Category="Crowd|Camera")
    void ConfigureView(const FVector& Center);

    /** 应用一套相机配置并立即重置镜头。 */
    UFUNCTION(BlueprintCallable, Category="Crowd|Camera")
    void ApplySettings(const FCrowdCameraSettings& NewSettings);

    /**
     * 请求转动一个 90° 观察面。Direction > 0 相当于 Q，< 0 相当于 E。
     * 正在旋转或 Direction==0 时返回 false；请求不会排队，避免连续按键造成失控镜头。
     */
    UFUNCTION(BlueprintCallable, Category="Crowd|Camera")
    bool RequestQuarterTurn(int32 Direction);

    /** 取消当前过渡，回到配置中的 InitialYaw。 */
    UFUNCTION(BlueprintCallable, Category="Crowd|Camera")
    void ResetView();

    /** 蓝图查询：当前是否正在进行 90° 过渡。 */
    UFUNCTION(BlueprintPure, Category="Crowd|Camera")
    bool IsRotating() const { return bRotating; }

    /** 蓝图查询：当前水平观察角（度，完成旋转后归一到 [-180, 180]）。 */
    UFUNCTION(BlueprintPure, Category="Crowd|Camera")
    float GetCurrentYaw() const { return CurrentYaw; }

    /** 由 Controller 每帧调用一次：在 Action 处理后推进缓动，在发布共享移动方向前完成。 */
    void AdvanceOrbit(float DeltaTime);

    /** 将屏幕/摇杆 XY 输入投影为当前镜头下的世界 XY 方向。 */
    FVector2D ScreenInputToWorld(FVector2D Input) const;

    /** 蓝图可编辑的默认轨道参数；Director 也可按关卡覆盖。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Crowd|Camera")
    FCrowdCameraSettings OrbitSettings;

    /** 接受一次有效转镜请求时广播，可在蓝图中播放音效或界面反馈。 */
    UPROPERTY(BlueprintAssignable, Category="Crowd|Camera")
    FCrowdViewRotationEvent OnRotationStarted;

    /** 镜头精确到达下一个 90° 观察面时广播。 */
    UPROPERTY(BlueprintAssignable, Category="Crowd|Camera")
    FCrowdViewRotationEvent OnRotationFinished;

    /** 实际渲染视图的透视 Camera 组件。 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Crowd")
    TObjectPtr<UCameraComponent> Camera;

private:
    /** 根据 CurrentYaw、Pitch 和 OrbitDistance 刷新 Camera 的相对变换。 */
    void ApplyOrbitPose();

    /** 当前显示角、过渡起点角、未归一化目标角；未归一化目标确保跨 ±180° 仍只走 90°。 */
    float CurrentYaw = -45.f;
    float StartYaw = -45.f;
    float TargetYaw = -45.f;

    /** 本次转镜累计时间（秒）。 */
    float Elapsed = 0.f;

    /** 为 true 时拒绝新的 Q/E 请求。 */
    bool bRotating = false;
};
