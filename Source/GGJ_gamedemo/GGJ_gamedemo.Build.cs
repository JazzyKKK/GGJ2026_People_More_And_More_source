// GGJ_gamedemo 运行时模块的依赖清单。
// 如果以后在 C++ 中使用新的 UE 模块（例如 UMG），需要在这里增加依赖后重新生成/编译工程。

using UnrealBuildTool;

public class GGJ_gamedemo : ModuleRules
{
	public GGJ_gamedemo(ReadOnlyTargetRules Target) : base(Target)
	{
		// 每个 .cpp 必须显式包含自己需要的头文件，也允许复用引擎共享预编译头。
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",          // 基础类型、数学、容器和日志。
			"CoreUObject",   // UObject、反射、垃圾回收和资源类型。
			"Engine",        // Actor、Component、World、相机和游戏框架。
			"InputCore",     // 键盘、手柄按键常量（EKeys）。
			"EnhancedInput", // Input Action / Mapping Context 输入系统。
			"PhysicsCore",   // 物理材质、约束及 Chaos 相关公共类型。
			"UMG"            // 倒计时等运行时界面。
		});

		// UMG 的原生控件最终由 Slate 渲染，仅在实现文件中使用。
		PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		// [PIXEL_WIND_VFX] 独立像素气流的自定义渲染组件。
		PrivateDependencyModuleNames.AddRange(new string[] { "RenderCore", "RHI" });
	}
}
