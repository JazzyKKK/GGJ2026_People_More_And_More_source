// 项目主模块入口。
// UE 启动时通过本文件注册名为 GGJ_gamedemo 的默认游戏模块。

#include "GGJ_gamedemo.h"
#include "Modules/ModuleManager.h"

// 当前不需要自定义 StartupModule/ShutdownModule，所以直接使用引擎提供的默认实现。
IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, GGJ_gamedemo, "GGJ_gamedemo");
