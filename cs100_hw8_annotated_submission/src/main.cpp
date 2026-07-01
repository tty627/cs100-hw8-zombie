// 入口文件：这里不写具体游戏逻辑，只负责创建 GameWorld 并交给老师提供的框架运行。
// code check 讲法：main.cpp 是程序入口，真正的游戏逻辑在 GameWorld 和各个 GameObject 子类中。

#include <memory> // 使用 std::shared_ptr 和 std::make_shared。

#include "pvz/Framework/GameManager.hpp" // 老师提供的游戏管理器，负责主循环、渲染、点击分发。
#include "pvz/GameWorld/GameWorld.hpp"   // 我自己实现的游戏世界类，负责 I, Zombie 的逻辑。

int main(int argc, char **argv) {
  // 用 shared_ptr<WorldBase> 接住 GameWorld，是典型的“基类指针指向派生类对象”。
  // 这里体现多态：框架只需要认识 WorldBase，但实际调用的是 GameWorld 重写后的 Init/Update/CleanUp。
  std::shared_ptr<WorldBase> world = std::make_shared<GameWorld>();

  // 进入老师框架的游戏循环。
  // 之后框架会自动调用 world->Init() 初始化，反复调用 world->Update() 更新每一帧，
  // 游戏结束或重开时调用 world->CleanUp() 清理。
  GameManager::Instance().Play(argc, argv, world);
}
