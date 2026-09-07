#pragma once

// 可持续操控的物理动画角色。
// ACharacter 负责可靠的行走/跳跃和胶囊导航；SkeletalMesh 的子骨骼启用 Chaos，
// UPhysicalAnimationComponent 再用“肌肉”把刚体拉回当前动画姿势。

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "PhysicsEngine/PhysicalAnimationComponent.h"
#include "TimerManager.h"
#include "GGJPhysicalAnimationCharacter.generated.h"

class UAnimationAsset;
class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class USpringArmComponent;
class AGGJCharacterGroupManager;
struct FInputActionValue;

/** 当前播放的单节点移动动画，主要用于避免每帧重复切换动画资源。 */
UENUM(BlueprintType)
enum class EGGJLocomotionState : uint8
{
    Idle,
    Walk,
    Run
};

/**
 * 官方第三人称模板式控制 + Physical Animation 的可操控布娃娃角色。
 *
 * C++ 保证初始化顺序、输入、碰撞和错误检查；蓝图子类负责选择模型、PhysicsAsset、
 * 动画资源与调参。这样以后美术替换同骨架模型时不需要复制事件图节点。
 */
UCLASS(Blueprintable)
class GGJ_GAMEDEMO_API AGGJPhysicalAnimationCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    AGGJPhysicalAnimationCharacter();

    /** 运行时更新 Idle/Walk/Run 动画和播放速度。 */
    virtual void Tick(float DeltaSeconds) override;

    /** 按当前蓝图配置重新建立物理动画；成功后 IsPhysicalAnimationReady 为 true。 */
    UFUNCTION(BlueprintCallable, Category="Physical Character|Physics")
    bool InitializePhysicalAnimation();

    /** 运行时整体调节肌肉强度；0 是完全无力，1 是蓝图参数原强度，允许大于 1。 */
    UFUNCTION(BlueprintCallable, Category="Physical Character|Physics")
    void SetMuscleStrength(float NewStrength);

    /** 清除骨骼速度并重新启用主动布娃娃，适合角色卡住后的关卡重置。 */
    UFUNCTION(BlueprintCallable, Category="Physical Character|Physics")
    void ResetPhysicalAnimation();

    /** 给指定骨骼一个瞬时冲量，便于按钮、机关或其他小人产生挤压反馈。 */
    UFUNCTION(BlueprintCallable, Category="Physical Character|Physics")
    void AddBoneImpulse(FName BoneName, FVector Impulse, bool bVelocityChange = true);

    /** 官方模板式移动入口：Right 为镜头右，Forward 为镜头前。 */
    UFUNCTION(BlueprintCallable, Category="Physical Character|Input")
    virtual void DoMove(float Right, float Forward);

    /** 群体管理器使用世界方向驱动未被 Controller 占有的成员。 */
    UFUNCTION(BlueprintCallable, Category="Physical Character|Group")
    void ApplySharedWorldMovement(FVector WorldMovement);

    /** 由 GroupManager 登记/注销时设置；蓝图一般不需要主动调用。 */
    void SetCharacterGroupManager(AGGJCharacterGroupManager* NewManager);

    UFUNCTION(BlueprintPure, Category="Physical Character|Group")
    AGGJCharacterGroupManager* GetCharacterGroupManager() const { return CharacterGroupManager.Get(); }

    /** 官方模板式自由环绕镜头入口。 */
    UFUNCTION(BlueprintCallable, Category="Physical Character|Input")
    virtual void DoLook(float Yaw, float Pitch);

    UFUNCTION(BlueprintCallable, Category="Physical Character|Input")
    virtual void DoJumpStart();

    UFUNCTION(BlueprintCallable, Category="Physical Character|Input")
    virtual void DoJumpEnd();

    /** Manager 调用的实际跳跃操作；与 DoJumpStart 分开以避免群体广播递归。 */
    UFUNCTION(BlueprintCallable, Category="Physical Character|Group")
    void ApplySharedJumpStart();

    UFUNCTION(BlueprintCallable, Category="Physical Character|Group")
    void ApplySharedJumpEnd();

    /** 群体相机向左/向右旋转 90°；分别由 Q/E 触发。 */
    UFUNCTION(BlueprintCallable, Category="Physical Character|Input")
    void RotateGroupCameraLeft();

    UFUNCTION(BlueprintCallable, Category="Physical Character|Input")
    void RotateGroupCameraRight();

    UFUNCTION(BlueprintPure, Category="Physical Character|Physics")
    bool IsPhysicalAnimationReady() const { return bPhysicalAnimationReady; }

    UFUNCTION(BlueprintPure, Category="Physical Character|Animation")
    EGGJLocomotionState GetLocomotionState() const { return LocomotionState; }

    UFUNCTION(BlueprintPure, Category="Physical Character|Components")
    USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

    UFUNCTION(BlueprintPure, Category="Physical Character|Components")
    UCameraComponent* GetFollowCamera() const { return FollowCamera; }

    /**
     * 控制本角色是否写入 Custom Depth / Stencil。
     * 群体相机中的遮挡轮廓后处理会读取它；关闭后该角色不再显示遮挡轮廓。
     */
    UFUNCTION(BlueprintCallable, Category="Physical Character|Occlusion Outline")
    void SetOcclusionOutlineEnabled(bool bEnabled);

    /**
     * 修改角色写入的 Stencil 编号，范围为 1~255。
     * 后处理材质默认识别编号 1；可用不同编号为特殊角色制作不同颜色。
     */
    UFUNCTION(BlueprintCallable, Category="Physical Character|Occlusion Outline")
    void SetOcclusionOutlineStencilValue(int32 NewStencilValue);

    UFUNCTION(BlueprintPure, Category="Physical Character|Occlusion Outline")
    bool IsOcclusionOutlineEnabled() const { return bOcclusionOutlineEnabled; }

    UFUNCTION(BlueprintPure, Category="Physical Character|Occlusion Outline")
    int32 GetOcclusionOutlineStencilValue() const { return OcclusionOutlineStencilValue; }

    /** 把动画姿势转换成物理马达参数；默认值精确复刻 Test_Lab2 的 phyAni 节点。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Physical Character|Physics")
    FPhysicalAnimationData DriveSettings;

    /** Test_Lab2 的 PhysicalAnimation 组件默认值为 5。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Physical Character|Physics", meta=(ClampMin="0.0"))
    float MuscleStrength = 5.f;

    /** 物理动画和子骨骼模拟从此骨骼开始；UE 导入 FBX 后会去掉 Mixamo 命名空间，当前资源使用 Hips。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Physical Character|Physics")
    FName DrivenBoneName = TEXT("Hips");

    /** 与 Test_Lab2 一致：马达包含起始骨，但物理模拟默认不包含它，让髋部跟随胶囊。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Physical Character|Physics")
    bool bIncludeDrivenBoneInSimulation = false;

    /** 关闭后角色仍可按第三人称方式移动和播动画，但不会启用布娃娃刚体。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Physical Character|Physics")
    bool bEnablePhysicalAnimationOnBeginPlay = true;

    /**
     * 动态生成角色后等待组件完成 Physics State 注册，再建立 Physical Animation 马达。
     * 设为 0 仍会至少等待到下一帧，避免 UE 5.6 在 Bodies 尚为空时发生数组越界。
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Physical Character|Physics", meta=(ClampMin="0.0"))
    float PhysicalAnimationInitializationDelay = 0.05f;

    /** 所有群体成员默认参与相机的“被建筑遮挡时显示轮廓”效果。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite,
        Category="Physical Character|Occlusion Outline")
    bool bOcclusionOutlineEnabled = true;

    /** 与后处理材质约定的角色类型编号；普通小人统一使用 1。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite,
        Category="Physical Character|Occlusion Outline",
        meta=(ClampMin="1", ClampMax="255", UIMin="1", UIMax="255"))
    int32 OcclusionOutlineStencilValue = 1;

    /** 原地或 Happy Idle 动画；不配置时保持参考姿势。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Physical Character|Animation")
    TObjectPtr<UAnimationAsset> IdleAnimation;

    /** 低速移动动画。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Physical Character|Animation")
    TObjectPtr<UAnimationAsset> WalkAnimation;

    /** 高速移动动画。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Physical Character|Animation")
    TObjectPtr<UAnimationAsset> RunAnimation;

    /** 小于此水平速度时播放 Idle。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Physical Character|Animation", meta=(ClampMin="0.0"))
    float IdleSpeedThreshold = 8.f;

    /** 大于此水平速度时播放 Run，其余播放 Walk。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Physical Character|Animation", meta=(ClampMin="0.0"))
    float RunSpeedThreshold = 320.f;

    /** 蓝图可替换正式 Input Assets；全部留空时 C++ 自动创建 WASD/鼠标/手柄映射。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Physical Character|Input")
    TObjectPtr<UInputMappingContext> InputMapping;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Physical Character|Input")
    TObjectPtr<UInputAction> MoveAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Physical Character|Input")
    TObjectPtr<UInputAction> LookAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Physical Character|Input")
    TObjectPtr<UInputAction> JumpAction;

protected:
    /** 先准备单节点动画，再建立物理动画约束。 */
    virtual void BeginPlay() override;

    /** 绑定 Enhanced Input Action。 */
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

    /** 移除本角色添加的 Mapping Context，避免切图后残留。 */
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    void HandleMove(const FInputActionValue& Value);
    void HandleLook(const FInputActionValue& Value);
    void EnsureInputAssets();
    void AddInputMapping();
    void UpdateLocomotionAnimation();
    void SchedulePhysicalAnimationInitialization();
    void TryInitializePhysicalAnimationDeferred();
    bool IsPhysicalBodyStateReady() const;
    bool IsAnimationCompatible(const UAnimationAsset* Animation) const;
    void PlayLocomotionAnimation(EGGJLocomotionState NewState, UAnimationAsset* Animation);
    void ApplyOcclusionOutlineSettings();

    /** 官方第三人称 Camera Boom：处理镜头距离和场景遮挡。 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Physical Character|Components", meta=(AllowPrivateAccess="true"))
    TObjectPtr<USpringArmComponent> CameraBoom;

    /** 挂在 Camera Boom 末端的跟随相机。 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Physical Character|Components", meta=(AllowPrivateAccess="true"))
    TObjectPtr<UCameraComponent> FollowCamera;

    /** 由 C++ 固定创建，蓝图只调上方 DriveSettings 和 MuscleStrength。 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Physical Character|Components", meta=(AllowPrivateAccess="true"))
    TObjectPtr<UPhysicalAnimationComponent> PhysicalAnimation;

private:
    bool bPhysicalAnimationReady = false;
    bool bEndingPlay = false;
    int32 PhysicalAnimationInitializationAttempts = 0;
    FTimerHandle PhysicalAnimationInitializationTimer;
    bool bInputMappingAdded = false;
    bool bOrbitInputMappingAdded = false;
    EGGJLocomotionState LocomotionState = EGGJLocomotionState::Idle;
    TObjectPtr<UAnimationAsset> CurrentAnimation;
    /** 记住最近拒绝的错误骨架动画，避免每帧重复刷相同警告。 */
    TObjectPtr<UAnimationAsset> LastRejectedAnimation;

    /** 弱引用避免 Character 和 Manager 相互拥有；Manager 销毁后自动失效。 */
    TWeakObjectPtr<AGGJCharacterGroupManager> CharacterGroupManager;

    /** Q/E 使用独立运行时 Context，避免覆盖美术或模板提供的移动 Input Mapping。 */
    UPROPERTY(Transient)
    TObjectPtr<UInputMappingContext> OrbitInputMapping;

    UPROPERTY(Transient)
    TObjectPtr<UInputAction> RotateLeftAction;

    UPROPERTY(Transient)
    TObjectPtr<UInputAction> RotateRightAction;
};
