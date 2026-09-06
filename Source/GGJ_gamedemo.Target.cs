// 游戏目标（Game Target）构建规则。
// 打包游戏或编译独立运行版本时，UnrealBuildTool 会从这里得知要构建哪些模块。

using UnrealBuildTool;
using System.Collections.Generic;

public class GGJ_gamedemoTarget : TargetRules
{
	public GGJ_gamedemoTarget(TargetInfo Target) : base(Target)
	{
		// 生成不包含编辑器功能的游戏程序。
		Type = TargetType.Game;
		// 使用 UE 5 当前的严格默认编译规则。
		DefaultBuildSettings = BuildSettingsVersion.V5;
		// 固定头文件包含顺序，避免升级引擎后出现隐式依赖差异。
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_6;
		// GGJ_gamedemo 是项目唯一的运行时 C++ 模块，具体依赖见 GGJ_gamedemo.Build.cs。
		ExtraModuleNames.Add("GGJ_gamedemo");
	}
}
