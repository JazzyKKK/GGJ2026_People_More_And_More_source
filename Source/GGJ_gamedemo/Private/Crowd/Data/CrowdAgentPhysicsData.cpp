// FCrowdPhysicsSettings 的运行时数据校验实现。
// 所有蓝图可编辑物理参数在进入 Chaos 之前都通过本文件净化。
#include "Crowd/Data/CrowdAgentPhysicsData.h"

// 所有外部可编辑浮点数在进入物理系统前都经过这里。
// 三元表达式先把 NaN/无穷替换为默认值，再 Clamp/Max 到运行安全范围。
FCrowdPhysicsSettings FCrowdPhysicsSettings::Sanitized() const
{
    FCrowdPhysicsSettings Result = *this;
    Result.Radius = FMath::Clamp(FMath::IsFinite(Radius) ? Radius : 30.f, 8.f, 100.f);
    Result.MassKg = FMath::Max(FMath::IsFinite(MassKg) ? MassKg : 1.f, 0.1f);
    Result.DriveAcceleration = FMath::Max(FMath::IsFinite(DriveAcceleration) ? DriveAcceleration : 1800.f, 0.f);
    Result.LinearDamping = FMath::Max(FMath::IsFinite(LinearDamping) ? LinearDamping : 6.f, 0.f);
    Result.SoftSpeedLimit = FMath::Max(FMath::IsFinite(SoftSpeedLimit) ? SoftSpeedLimit : 300.f, 1.f);
    return Result;
}
