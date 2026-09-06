#pragma once

// 单屏关卡的人群初始化入口。
// 每张玩法地图放置且只放置一个 Director，用它配置人数、出生方式和该关镜头中心。

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Crowd/Data/CrowdAgentPhysicsData.h"
#include "Crowd/Data/CrowdCameraSettings.h"
#include "CrowdLevelDirector.generated.h"

class ACrowdAgent;
class ACrowdSpawnPoint;
class UArrowComponent;

/**
 * 连接“关卡蓝图配置”和“运行时人口系统”的场景 Actor。
 * 它只生成/重置人群并配置相机，不生成或修改地板、墙、机关、灯光等关卡几何。
 */
UCLASS(Blueprintable)
class GGJ_GAMEDEMO_API ACrowdLevelDirector : public AActor
{
    GENERATED_BODY()
public:
    /** 创建绿色出生原点箭头，并用原生 ACrowdAgent 作为未配置时的兜底类。 */
    ACrowdLevelDirector();

    /** 等场景与 GameMode 默认 Pawn 就绪后，于下一帧配置相机并生成人群。 */
    virtual void BeginPlay() override;

    /** 销毁旧人口并按当前关卡配置重新生成；任何一个出生失败都会回滚为空。 */
    UFUNCTION(BlueprintCallable, Category="Crowd")
    bool ResetCrowd();

    /** 要生成的小人类；通常在 BP_CrowdLevelDirector 中选择 BP_CrowdAgent。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Crowd|Population")
    TSubclassOf<ACrowdAgent> AgentClass;

    /** 可选共享物理 Data Asset；非空时优先使用其中的 Settings。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Crowd|Population")
    TObjectPtr<UCrowdAgentPhysicsData> PhysicsData;

    /** PhysicsData 为空时采用的关卡内嵌参数。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Crowd|Population", meta=(EditCondition="PhysicsData == nullptr"))
    FCrowdPhysicsSettings DefaultPhysics;

    /** 非空时每个标记生成一人并忽略 InitialCount；为空时使用下方自动阵列。 */
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Crowd|Population")
    TArray<TObjectPtr<ACrowdSpawnPoint>> SpawnPoints;

    /** 自动阵列总人数，当前基础版本限制为 1～64。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Crowd|Grid", meta=(ClampMin="1", ClampMax="64"))
    int32 InitialCount = 12;

    /** 自动阵列每行列数；会根据实际人数再次限制。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Crowd|Grid", meta=(ClampMin="1", ClampMax="64"))
    int32 GridColumns = 4;

    /** 自动阵列中心距；过小时会按碰撞直径自动增大，避免开局重叠。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Crowd|Grid", meta=(ClampMin="1"))
    float GridSpacing = 90.f;

    /** 未指定 CameraTarget 时，相对 Director 位置的备用镜头中心；Director.Z 同时定义移动平面。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Crowd|Camera")
    FVector CameraCenterOffset = FVector::ZeroVector;

    /** 可选的场地观察中心 Actor/TargetPoint；只读取其位置。 */
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Crowd|Camera")
    TObjectPtr<AActor> CameraTarget;

    /** 勾选后使用当前关卡的 CameraSettingsOverride，而不是 Camera 蓝图默认值。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Crowd|Camera")
    bool bOverrideCameraSettings = false;

    /** 当前关卡专用的镜头距离、角度、FOV 和缓动参数。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Crowd|Camera", meta=(EditCondition="bOverrideCameraSettings", EditConditionHides))
    FCrowdCameraSettings CameraSettingsOverride;

    /** 查找 Player 0 的 CrowdCameraPawn，并应用关卡覆盖与观察中心。 */
    UFUNCTION(BlueprintCallable, Category="Crowd|Camera")
    void ConfigureCamera();

    /** 绿色编辑器箭头，同时作为没有 SpawnPoints 时的自动阵列中心。 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Crowd")
    TObjectPtr<UArrowComponent> SpawnOrigin;

    /** 最近一次 ResetCrowd 是否完整成功；可供关卡蓝图或调试 UI 查询。 */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Crowd|Status")
    bool bInitialized = false;

private:
    /** 同时写日志与屏幕红字，向关卡设计师报告可操作的配置错误。 */
    void ReportError(const FString& Message);
};
