#ifndef GAMEWORLD_HPP__
#define GAMEWORLD_HPP__

#include <array>
#include <memory>
#include <string>
#include <vector>

#include "pvz/Framework/WorldBase.hpp"

#include "pvz/GameObject/GameObject.hpp"

#include "pvz/Framework/TextBase.hpp"
#include "pvz/utils.hpp"

class Pea;
class Plant;
class Zombie;

// GameWorld 是整个游戏世界的核心类，继承老师框架提供的 WorldBase。
// 框架只知道 WorldBase，但实际运行时会调用 GameWorld 重写的 Init/Update/CleanUp。
// code check 总述：GameWorld 负责保存全局状态、管理所有 GameObject、处理对象之间的交互。
class GameWorld : public WorldBase {
public:
  GameWorld();
  ~GameWorld() override = default;

  // 初始化一局游戏：清理旧数据、重置阳光/阶段/脑子、创建 UI 和场景对象、生成植物防线。
  void Init() override;

  // 每一帧调用一次：更新卡牌、更新对象、加入新对象、删除死亡对象、检查胜负。
  LevelStatus Update() override;

  // 清理当前关卡中的所有对象和辅助记录。
  void CleanUp() override;

  // 行列坐标和屏幕坐标之间的转换函数。
  // 这些函数只依赖传入参数和常量，不依赖某个 GameWorld 对象本身，所以设计成 static。
  static int RowToY(int row);
  static int ColToX(int col);
  static int XToCol(int x);
  static int CardX(int cardIndex);

  // QueueObject：在一帧更新过程中创建的新对象先放入 m_newObjects。
  // AddObject：初始化阶段可以直接加入 m_objects。
  // 区分这两个函数是为了避免遍历 m_objects 时直接修改 m_objects。
  void QueueObject(std::unique_ptr<GameObject> object);
  void AddObject(std::unique_ptr<GameObject> object);

  // 查询植物/僵尸/豌豆碰撞相关接口。
  // 具体对象不直接访问容器，而是通知 GameWorld，由 GameWorld 统一查找。
  Plant* GetPlantAt(int row, int col) const;
  Plant* FindPlantCollidingWithZombie(const Zombie& zombie) const;
  Plant* FindPlantForPoleJump(const Zombie& zombie) const;
  Zombie* FindFirstZombieOnRight(int row, int col);
  Zombie* FindZombieHitByPea(const Pea& pea);
  bool HasZombieOnRight(int row, int col) const;

  // 创建对象或改变对象状态的接口。
  // 这些函数被植物、僵尸、阳光、投放格等对象调用。
  void CreatePea(int row, int col, int xOffset = 0);
  void CreateZombie(ZombieType type, int row, int col);
  void CreateSun(int x, int y, int value);
  void DamagePlantAt(int row, int col, int damage);

  // 脑子相关：僵尸碰到脑子时调用 TryEatBrain，BrainTarget 显示对象通过 IsBrainAlive 判断自己是否该消失。
  bool TryEatBrain(int row, int x, int y, int width, int height);
  bool IsBrainAlive(int row) const;

  // 卡牌和投放相关。
  // CardButton 调 TrySelectCard，DeploymentCell 调 TryDeployZombie。
  void TrySelectCard(int cardIndex);
  void TryDeployZombie(int row, int col);
  bool IsCardSelected(int cardIndex) const;
  bool IsCardCooling(int cardIndex) const;
  void CollectSun(int value);

  // 红线和进度条显示对象需要查询当前显示位置/图片。
  int GetRedLineX() const;
  ImageID GetProgressImage() const;

private:
  // 生成植物防线时使用的内部枚举。
  // 只在 GameWorld 内部使用，所以放在 private。
  enum class PlantChoice {
    SUNFLOWER,
    PEASHOOTER,
    REPEATER,
    WALLNUT
  };

  // 初始化和 UI 辅助函数。
  void ResetLevelData();
  void ResetPlantGrid();
  void InitCards();
  void CreateSceneObjects();
  void CreateTexts();
  void UpdateTexts();

  // 每帧更新流程的四个核心步骤。
  void UpdateCards();
  void UpdateObjects();
  void FlushNewObjects();
  void RemoveDeadObjects();

  // 阶段推进和植物生成逻辑。
  void CheckStageClear();
  void EnterNextStage();
  void ClearStageObjects();
  void GeneratePlantsForStage();
  void GenerateStageOnePlants();
  void GenerateStageTwoPlants();
  void GenerateStageThreePlants();
  void GenerateStageFourPlants();
  void GenerateFinalStagePlants();
  bool TryCreatePlantAt(int row, int col, PlantChoice choice);
  bool CreateRandomPlantInRow(int row, PlantChoice choice);

  // 胜负和合法性判断。
  void CheckGameOver();
  bool CanStillAct() const;
  bool HasCollectableSun() const;
  bool HasUsefulZombie() const;
  bool HasAliveBrain() const;
  bool CanUseCard(int cardIndex) const;
  bool CanPlaceZombieAt(int row, int col) const;
  bool IsInsideGrid(int row, int col) const;
  bool IsRectOverlapping(int x1, int y1, int w1, int h1,
                         int x2, int y2, int w2, int h2) const;
  std::string SelectedCardText() const;

  // 主对象容器：保存所有会显示/更新/点击的游戏对象。
  // 使用 unique_ptr 表示 GameWorld 独占这些对象的生命周期。
  std::vector<std::unique_ptr<GameObject>> m_objects;

  // 新对象临时队列：对象 Update 期间创建的新对象先进入这里。
  // 等本帧所有对象都更新完，再通过 FlushNewObjects 移入 m_objects。
  std::vector<std::unique_ptr<GameObject>> m_newObjects;

  // 文本对象容器：保存阳光数、阶段、当前选择等文字。
  std::vector<std::unique_ptr<TextBase>> m_texts;

  // 植物格子索引表：m_plantAt[row][col] 指向该格子的植物。
  // 注意它不是 owning pointer，不负责释放植物；真正所有权仍在 m_objects 的 unique_ptr 里。
  // 这个表是为了快速判断某个格子是否有植物，避免每次都遍历全部对象。
  std::array<std::array<Plant*, GAME_COLS>, GAME_ROWS> m_plantAt;

  // 每一行脑子是否还活着；所有行都变 false 表示当前阶段完成。
  std::array<bool, GAME_ROWS> m_brainAlive;

  // 僵尸卡牌状态数组，长度为 CARD_COUNT。
  std::array<CardState, CARD_COUNT> m_cards;

  // 下面三个是 m_texts 中对象的裸指针缓存。
  // 它们不拥有 TextBase，只是方便快速 SetText；真正所有权在 m_texts。
  TextBase* m_sunText;
  TextBase* m_stageText;
  TextBase* m_selectedText;

  // 玩家当前阳光。
  int m_sun;

  // 当前阶段，从 1 开始。
  int m_stage;

  // 当前选中的卡牌下标；-1 表示没有选中。
  int m_selectedCard;

  // 红线所在列。每过一阶段向右推进一列。
  int m_redLineCol;

  // 当前游戏状态：进行中、胜利、失败。
  LevelStatus m_status;

};

#endif // !GAMEWORLD_HPP__
