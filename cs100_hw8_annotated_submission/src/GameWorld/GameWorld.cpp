#include "pvz/GameWorld/GameWorld.hpp"

#include <cstdlib>

#include "pvz/GameObject/Plant.hpp"
#include "pvz/GameObject/SimpleObjects.hpp"
#include "pvz/GameObject/Zombie.hpp"

namespace {

// 根据僵尸类型返回对应的卡牌图片。
// 这是文件内部辅助函数，只在本 cpp 使用，所以放在匿名 namespace 里，避免污染全局命名空间。
ImageID CardImage(ZombieType type) {
  switch (type) {
  case ZombieType::REGULAR:
    return ImageID::ZOMBIE_CARD_REGULAR;
  case ZombieType::BUCKET:
    return ImageID::ZOMBIE_CARD_BUCKET;
  case ZombieType::POLE_VAULTING:
    return ImageID::ZOMBIE_CARD_POLE;
  }
  return ImageID::ZOMBIE_CARD_REGULAR;
}

// 根据僵尸类型返回显示在 UI 文本里的名字。
std::string ZombieName(ZombieType type) {
  switch (type) {
  case ZombieType::REGULAR:
    return "Regular";
  case ZombieType::BUCKET:
    return "Bucket";
  case ZombieType::POLE_VAULTING:
    return "Pole";
  }
  return "None";
}

} // namespace

// GameWorld 构造函数。
// 这里只做最基本的成员初始化；真正每一局游戏的初始化放在 Init() 中。
// 因为失败后重开时，框架可能复用同一个 GameWorld 对象，所以 Init() 必须能重新初始化一局。
GameWorld::GameWorld()
    : m_sunText(nullptr),
      m_stageText(nullptr),
      m_selectedText(nullptr),
      m_sun(150),
      m_stage(1),
      m_selectedCard(-1),
      m_redLineCol(INITIAL_ZOMBIE_DEPLOYMENT_START_COL),
      m_status(LevelStatus::ONGOING) {
  ResetPlantGrid();
  m_brainAlive.fill(true);
  InitCards();
}

// 初始化一局游戏。
// code check 主线：清理旧对象 -> 重置关卡数据 -> 初始化卡牌 -> 创建 UI/场景 -> 生成植物 -> 更新文本。
void GameWorld::Init() {
  // 防止上一局或旧状态残留。
  CleanUp();
  // 重置阳光、阶段、脑子、红线、选择状态等。
  ResetLevelData();
  // 设置三张僵尸卡的类型、价格和冷却。
  InitCards();
  // 创建阳光数、阶段信息、当前选择等文字。
  CreateTexts();
  // 创建背景、脑子、红线、卡牌、冷却遮罩、投放格等基础对象。
  CreateSceneObjects();
  // 根据当前阶段生成植物防线。
  GeneratePlantsForStage();
  // 把文本内容刷新成当前状态。
  UpdateTexts();
}

// 每一帧的核心更新函数。
// 框架会反复调用这个函数，并根据返回值判断游戏是否继续、胜利或失败。
LevelStatus GameWorld::Update() {
  // 如果已经胜利或失败，就不再继续更新游戏逻辑。
  if (m_status != LevelStatus::ONGOING) {
    return m_status;
  }

  // 1. 更新卡牌冷却。
  UpdateCards();
  // 2. 通过虚函数调用所有对象自己的 Update。
  UpdateObjects();
  // 3. 把这一帧中新创建的对象加入主对象容器。
  FlushNewObjects();
  // 4. 删除被标记死亡的对象。
  RemoveDeadObjects();
  // 5. 检查是否吃掉了所有脑子，是否需要进入下一阶段或胜利。
  CheckStageClear();
  if (m_status == LevelStatus::ONGOING) {
    // 6. 如果还没胜利，再检查是否失败。
    CheckGameOver();
  }
  // 7. 刷新阳光数、阶段、选择状态等文字显示。
  UpdateTexts();
  return m_status;
}

// 清理当前 GameWorld 保存的对象和辅助指针。
// vector<unique_ptr<...>>::clear() 会自动释放对象，不需要手动 delete。
void GameWorld::CleanUp() {
  m_newObjects.clear();
  m_objects.clear();
  m_texts.clear();
  ResetPlantGrid();
  m_sunText = nullptr;
  m_stageText = nullptr;
  m_selectedText = nullptr;
}

// 行号转换为屏幕 y 坐标。
int GameWorld::RowToY(int row) {
  return FIRST_ROW_CENTER + row * LAWN_GRID_HEIGHT;
}

// 列号转换为屏幕 x 坐标。
int GameWorld::ColToX(int col) {
  return FIRST_COL_CENTER + col * LAWN_GRID_WIDTH;
}

// 屏幕 x 坐标转换为最近的草坪列号。
// 最后会 clamp 到 [0, GAME_COLS - 1]，防止越界。
int GameWorld::XToCol(int x) {
  int col = (x - FIRST_COL_CENTER + LAWN_GRID_WIDTH / 2) / LAWN_GRID_WIDTH;
  if (col < 0) {
    col = 0;
  }
  if (col >= GAME_COLS) {
    col = GAME_COLS - 1;
  }
  return col;
}

// 根据卡牌下标计算卡牌 UI 的 x 坐标。
int GameWorld::CardX(int cardIndex) {
  return ZOMBIE_CARD_FIRST_X + cardIndex * ZOMBIE_CARD_SPACING;
}

// 一帧更新过程中创建的新对象先进入 m_newObjects。
// 这样可以避免在遍历 m_objects 时直接 push_back 导致 vector 迭代/索引逻辑不稳定。
void GameWorld::QueueObject(std::unique_ptr<GameObject> object) {
  m_newObjects.push_back(std::move(object));
}

// 初始化阶段可以直接把对象加入主容器。
void GameWorld::AddObject(std::unique_ptr<GameObject> object) {
  m_objects.push_back(std::move(object));
}

// 获取某个格子的植物。
// 如果坐标越界、没有植物、或者植物已经死亡，都返回 nullptr。
Plant* GameWorld::GetPlantAt(int row, int col) const {
  if (!IsInsideGrid(row, col)) {
    return nullptr;
  }
  Plant* plant = m_plantAt[row][col];
  if (plant == nullptr || plant->IsDead()) {
    return nullptr;
  }
  return plant;
}

// 查找与某个僵尸发生碰撞的植物。
// 僵尸只会和同一行植物互动，所以先拿 zombie.GetRow()。
Plant* GameWorld::FindPlantCollidingWithZombie(const Zombie& zombie) const {
  const int row = zombie.GetRow();
  if (row < 0 || row >= GAME_ROWS) {
    return nullptr;
  }

  for (int col = 0; col < GAME_COLS; ++col) {
    Plant* plant = GetPlantAt(row, col);
    if (plant != nullptr &&
        IsRectOverlapping(zombie.GetX(), zombie.GetY(), zombie.GetWidth(),
                          zombie.GetHeight(), plant->GetX(), plant->GetY(),
                          plant->GetWidth(), plant->GetHeight())) {
      return plant;
    }
  }
  return nullptr;
}

// 撑杆跳僵尸专用：检测前方是否有可以跳过的植物。
// testX 比当前僵尸位置更靠左一点，相当于提前探测前方障碍。
Plant* GameWorld::FindPlantForPoleJump(const Zombie& zombie) const {
  const int row = zombie.GetRow();
  if (row < 0 || row >= GAME_ROWS) {
    return nullptr;
  }

  const int testX = zombie.GetX() - 40;
  for (int col = 0; col < GAME_COLS; ++col) {
    Plant* plant = GetPlantAt(row, col);
    if (plant != nullptr &&
        IsRectOverlapping(testX, zombie.GetY(), zombie.GetWidth(),
                          zombie.GetHeight(), plant->GetX(), plant->GetY(),
                          plant->GetWidth(), plant->GetHeight())) {
      return plant;
    }
  }
  return nullptr;
}

// 查找某行某列右侧距离最近的僵尸。
// 射击植物会用这个逻辑判断目标方向。
Zombie* GameWorld::FindFirstZombieOnRight(int row, int col) {
  const int plantX = ColToX(col);
  Zombie* best = nullptr;
  for (const auto& object : m_objects) {
    // AsZombie() 返回 nullptr 表示这个对象不是僵尸。
    Zombie* zombie = object->AsZombie();
    if (zombie == nullptr || zombie->IsDead()) {
      continue;
    }
    if (zombie->GetRow() == row && zombie->GetX() > plantX &&
        (best == nullptr || zombie->GetX() < best->GetX())) {
      best = zombie;
    }
  }
  return best;
}

// 查找被豌豆命中的僵尸。
// 如果同时重叠多个僵尸，选择 x 最小的那个，也就是豌豆最先碰到的目标。
Zombie* GameWorld::FindZombieHitByPea(const Pea& pea) {
  Zombie* best = nullptr;
  for (const auto& object : m_objects) {
    Zombie* zombie = object->AsZombie();
    if (zombie == nullptr || zombie->IsDead() || zombie->GetRow() != pea.GetRow()) {
      continue;
    }
    if (IsRectOverlapping(pea.GetX(), pea.GetY(), pea.GetWidth(), pea.GetHeight(),
                          zombie->GetX(), zombie->GetY(), zombie->GetWidth(),
                          zombie->GetHeight()) &&
        (best == nullptr || zombie->GetX() < best->GetX())) {
      best = zombie;
    }
  }
  return best;
}

// 判断指定植物右侧是否存在僵尸。
// Peashooter/Repeater 只有在右侧有僵尸时才发射豌豆。
bool GameWorld::HasZombieOnRight(int row, int col) const {
  const int plantX = ColToX(col);
  for (const auto& object : m_objects) {
    const Zombie* zombie = object->AsZombie();
    if (zombie != nullptr && !zombie->IsDead() && zombie->GetRow() == row &&
        zombie->GetX() > plantX) {
      return true;
    }
  }
  return false;
}

// 创建豌豆。这里使用 QueueObject，而不是 AddObject。
// 因为豌豆通常是在植物 Update 过程中被创建的，此时可能正在遍历 m_objects。
void GameWorld::CreatePea(int row, int col, int xOffset) {
  QueueObject(std::make_unique<Pea>(this, row, col, xOffset));
}

// 根据僵尸类型创建具体僵尸对象。
// 这里体现“选择类型 -> 创建对应派生类对象”的工厂式逻辑。
void GameWorld::CreateZombie(ZombieType type, int row, int col) {
  switch (type) {
  case ZombieType::REGULAR:
    QueueObject(std::make_unique<RegularZombie>(this, row, col));
    break;
  case ZombieType::BUCKET:
    QueueObject(std::make_unique<BucketZombie>(this, row, col));
    break;
  case ZombieType::POLE_VAULTING:
    QueueObject(std::make_unique<PoleVaultingZombie>(this, row, col));
    break;
  }
}

// 创建阳光。向日葵死亡时会调用这里。
void GameWorld::CreateSun(int x, int y, int value) {
  QueueObject(std::make_unique<SunPickup>(this, x, y, value));
}

// 对某个格子的植物造成伤害。
// 僵尸不知道植物对象具体由谁管理，只把 row/col 交给 GameWorld。
void GameWorld::DamagePlantAt(int row, int col, int damage) {
  Plant* plant = GetPlantAt(row, col);
  if (plant != nullptr) {
    plant->TakeDamage(damage);
  }
}

// 尝试吃掉某一行的脑子。
// 僵尸传入自己的矩形，GameWorld 判断是否与该行脑子的矩形重叠。
bool GameWorld::TryEatBrain(int row, int x, int y, int width, int height) {
  if (row < 0 || row >= GAME_ROWS || !m_brainAlive[row]) {
    return false;
  }
  if (IsRectOverlapping(x, y, width, height, 35, RowToY(row), 32, 31)) {
    // 这一行脑子被吃掉。
    m_brainAlive[row] = false;
    return true;
  }
  return false;
}

// 查询某一行脑子是否还活着。
// BrainTarget 对象通过这个函数决定自己是否该消失。
bool GameWorld::IsBrainAlive(int row) const {
  if (row < 0 || row >= GAME_ROWS) {
    return false;
  }
  return m_brainAlive[row];
}

// 尝试选中一张卡牌。
// 如果卡牌冷却中或阳光不够，CanUseCard 会返回 false，点击无效。
void GameWorld::TrySelectCard(int cardIndex) {
  if (!CanUseCard(cardIndex)) {
    return;
  }

  // 先取消所有卡牌选中状态，保证同一时间最多选中一张。
  for (int i = 0; i < CARD_COUNT; ++i) {
    m_cards[i].selected = false;
  }
  m_cards[cardIndex].selected = true;
  m_selectedCard = cardIndex;
  UpdateTexts();
}

// 尝试在某个格子投放当前选中的僵尸。
// 真正扣阳光、进入冷却、生成僵尸都发生在投放成功之后。
void GameWorld::TryDeployZombie(int row, int col) {
  if (m_selectedCard < 0 || m_selectedCard >= CARD_COUNT) {
    return;
  }
  // 不可用卡牌、非法格子、红线左侧、出生点被占用时都不能投放。
  if (!CanUseCard(m_selectedCard) || !CanPlaceZombieAt(row, col)) {
    return;
  }

  CardState& card = m_cards[m_selectedCard];
  // 创建僵尸，扣阳光，设置冷却，取消选中。
  CreateZombie(card.type, row, col);
  m_sun -= card.cost;
  card.currentCooldown = card.cooldown;
  card.selected = false;
  m_selectedCard = -1;
  UpdateTexts();
}

// 判断某张卡是否正在冷却。
bool GameWorld::IsCardCooling(int cardIndex) const {
  if (cardIndex < 0 || cardIndex >= CARD_COUNT) {
    return false;
  }
  return m_cards[cardIndex].currentCooldown > 0;
}

// 判断某张卡是否被选中。
bool GameWorld::IsCardSelected(int cardIndex) const {
  if (cardIndex < 0 || cardIndex >= CARD_COUNT) {
    return false;
  }
  return m_cards[cardIndex].selected;
}

// 收集阳光。SunPickup::OnClick() 会调用这里。
void GameWorld::CollectSun(int value) {
  m_sun += value;
  UpdateTexts();
}

// 根据当前红线列计算红线对象的屏幕 x 坐标。
// firstDeployCol 表示普通僵尸最早可以投放的列，所以红线画在它左边。
int GameWorld::GetRedLineX() const {
  const int firstDeployCol = m_redLineCol + ZOMBIE_DEPLOYMENT_BUFFER_COLS;
  return ColToX(firstDeployCol) - LAWN_GRID_WIDTH / 2;
}

// 根据当前 stage 选择进度条图片。
ImageID GameWorld::GetProgressImage() const {
  int stageIndex = m_stage - 1;
  if (stageIndex < 0) {
    stageIndex = 0;
  }
  if (stageIndex > 7) {
    stageIndex = 7;
  }
  return static_cast<ImageID>(
      static_cast<int>(ImageID::PROGRESS_METER_STAGE_1) + stageIndex);
}

// 重置一局游戏的核心数据。
// 注意这里不直接创建对象，只重置数值状态。
void GameWorld::ResetLevelData() {
  m_sun = 150;
  m_stage = 1;
  m_selectedCard = -1;
  m_redLineCol = INITIAL_ZOMBIE_DEPLOYMENT_START_COL;
  m_status = LevelStatus::ONGOING;
  m_brainAlive.fill(true);
  ResetPlantGrid();
}

// 清空植物格子索引表。
// 这里只清空非拥有指针，不删除植物；植物对象由 m_objects 的 unique_ptr 管理。
void GameWorld::ResetPlantGrid() {
  for (int row = 0; row < GAME_ROWS; ++row) {
    for (int col = 0; col < GAME_COLS; ++col) {
      m_plantAt[row][col] = nullptr;
    }
  }
}

// 初始化三张僵尸卡。
// 字段顺序：类型、价格、总冷却、当前冷却、是否选中。
void GameWorld::InitCards() {
  m_cards[0] = CardState{ZombieType::REGULAR, 50, 120, 0, false};
  m_cards[1] = CardState{ZombieType::BUCKET, 125, 180, 0, false};
  m_cards[2] = CardState{ZombieType::POLE_VAULTING, 75, 160, 0, false};
}

// 创建场景中的基础对象。
// 这些对象包括背景、红线、进度条、脑子、卡牌、冷却遮罩和所有投放格。
void GameWorld::CreateSceneObjects() {
  AddObject(std::make_unique<Background>(this));
  AddObject(std::make_unique<RedLine>(this));
  AddObject(std::make_unique<ProgressMeter>(this));

  // 每行一个脑子目标。
  for (int row = 0; row < GAME_ROWS; ++row) {
    AddObject(std::make_unique<BrainTarget>(this, row));
  }

  // 每张僵尸卡都有一个按钮对象和一个冷却遮罩对象。
  for (int i = 0; i < CARD_COUNT; ++i) {
    AddObject(std::make_unique<CardButton>(this, i, CardImage(m_cards[i].type)));
    AddObject(std::make_unique<CooldownMask>(this, i));
  }

  // 每个草坪格子创建一个不可见投放格。
  // 点击格子时，DeploymentCell 会通知 GameWorld 尝试投放僵尸。
  for (int row = 0; row < GAME_ROWS; ++row) {
    for (int col = 0; col < GAME_COLS; ++col) {
      AddObject(std::make_unique<DeploymentCell>(this, row, col));
    }
  }
}

// 创建文字 UI。
// m_texts 用 unique_ptr 拥有 TextBase，m_sunText/m_stageText/m_selectedText 只是方便访问的缓存指针。
void GameWorld::CreateTexts() {
  m_texts.clear();

  // 阳光数文字。
  m_texts.push_back(std::make_unique<TextBase>(SUN_COUNTER_X, SUN_COUNTER_Y,
                                               "", 0.0, 0.0, 0.0, true));
  m_sunText = m_texts.back().get();

  // 阶段和剩余脑子文字。
  m_texts.push_back(std::make_unique<TextBase>(620, 565, "", 1.0, 1.0, 1.0,
                                               false));
  m_stageText = m_texts.back().get();

  // 当前选中卡牌文字。
  m_texts.push_back(std::make_unique<TextBase>(430, 565, "", 1.0, 1.0, 0.6,
                                               false));
  m_selectedText = m_texts.back().get();
}

// 刷新所有文字显示。
// 每次阳光、阶段、脑子、选中状态变化后都可以调用。
void GameWorld::UpdateTexts() {
  if (m_sunText != nullptr) {
    m_sunText->SetText(std::to_string(m_sun));
  }
  if (m_stageText != nullptr) {
    // 统计还活着的脑子数量。
    int brains = 0;
    for (int row = 0; row < GAME_ROWS; ++row) {
      if (m_brainAlive[row]) {
        ++brains;
      }
    }
    m_stageText->SetText("Stage " + std::to_string(m_stage) + "/" +
                         std::to_string(TOTAL_ROUNDS) + "  Brains " +
                         std::to_string(brains));
  }
  if (m_selectedText != nullptr) {
    m_selectedText->SetText(SelectedCardText());
  }
}

// 每帧减少卡牌冷却时间。
void GameWorld::UpdateCards() {
  for (int i = 0; i < CARD_COUNT; ++i) {
    if (m_cards[i].currentCooldown > 0) {
      --m_cards[i].currentCooldown;
    }
  }
}

// 更新所有主容器中的对象。
// 这里调用的是虚函数 Update，因此实际执行的是各个子类自己的逻辑。
void GameWorld::UpdateObjects() {
  for (std::size_t i = 0; i < m_objects.size(); ++i) {
    if (!m_objects[i]->IsDead()) {
      m_objects[i]->Update();
    }
  }
}

// 把本帧中新创建的对象从临时队列移动到主容器。
// std::move 表示转移 unique_ptr 的所有权。
void GameWorld::FlushNewObjects() {
  for (std::size_t i = 0; i < m_newObjects.size(); ++i) {
    m_objects.push_back(std::move(m_newObjects[i]));
  }
  m_newObjects.clear();
}

// 删除死亡对象。
// 对象死亡时只是 MarkDead；统一删除放在这里做，避免 Update 过程中容器被随意修改。
void GameWorld::RemoveDeadObjects() {
  for (auto iter = m_objects.begin(); iter != m_objects.end();) {
    GameObject* object = iter->get();
    if (object->IsDead()) {
      // 如果死亡对象是植物，还要清空 m_plantAt 中对应的非拥有指针，避免留下悬空指针。
      Plant* plant = object->AsPlant();
      if (plant != nullptr && IsInsideGrid(plant->GetRow(), plant->GetCol()) &&
          m_plantAt[plant->GetRow()][plant->GetCol()] == plant) {
        m_plantAt[plant->GetRow()][plant->GetCol()] = nullptr;
      }
      // erase 会销毁 unique_ptr，从而自动释放对象。
      // erase 返回下一个有效迭代器，所以这里不能再 ++iter。
      iter = m_objects.erase(iter);
    } else {
      ++iter;
    }
  }
}

// 检查当前阶段是否完成。
// 如果还有脑子活着，说明阶段没完成；如果所有脑子都被吃掉，就进入下一阶段或胜利。
void GameWorld::CheckStageClear() {
  if (m_status != LevelStatus::ONGOING || HasAliveBrain()) {
    return;
  }

  // 最后一阶段完成后，游戏胜利。
  if (m_stage >= TOTAL_ROUNDS) {
    m_status = LevelStatus::WINNING;
    return;
  }

  // 否则进入下一阶段，红线右移一列。
  ++m_stage;
  ++m_redLineCol;
  EnterNextStage();
}

// 进入下一阶段。
// 清空上一阶段对象，重置阳光和脑子，重新创建场景和植物。
void GameWorld::EnterNextStage() {
  ClearStageObjects();
  m_sun = 150;
  m_selectedCard = -1;
  m_brainAlive.fill(true);
  InitCards();
  CreateTexts();
  CreateSceneObjects();
  GeneratePlantsForStage();
  UpdateTexts();
}

// 清空当前阶段对象和辅助指针。
// 和 CleanUp 类似，但不重置 stage/redLine，因为进入下一阶段要保留已经推进后的阶段信息。
void GameWorld::ClearStageObjects() {
  m_newObjects.clear();
  m_objects.clear();
  m_texts.clear();
  ResetPlantGrid();
  m_sunText = nullptr;
  m_stageText = nullptr;
  m_selectedText = nullptr;
}

// 根据当前阶段选择植物生成策略。
// 当前实现是按阶段预设布局，保证难度逐步增加。
void GameWorld::GeneratePlantsForStage() {
  if (m_stage <= 1) {
    GenerateStageOnePlants();
  } else if (m_stage == 2) {
    GenerateStageTwoPlants();
  } else if (m_stage == 3) {
    GenerateStageThreePlants();
  } else if (m_stage == 4) {
    GenerateStageFourPlants();
  } else {
    GenerateFinalStagePlants();
  }
}

// 第一阶段：每行红线位置放一个向日葵，难度最低。
void GameWorld::GenerateStageOnePlants() {
  for (int row = 0; row < GAME_ROWS; ++row) {
    TryCreatePlantAt(row, m_redLineCol, PlantChoice::SUNFLOWER);
  }
}

// 第二阶段：后排向日葵，前排加入豌豆射手和一个坚果。
void GameWorld::GenerateStageTwoPlants() {
  const int frontCol = m_redLineCol;
  const int sunCol = m_redLineCol - 1;

  for (int row = 0; row < GAME_ROWS; ++row) {
    TryCreatePlantAt(row, sunCol, PlantChoice::SUNFLOWER);
  }

  TryCreatePlantAt(0, frontCol, PlantChoice::PEASHOOTER);
  TryCreatePlantAt(1, frontCol, PlantChoice::WALLNUT);
  TryCreatePlantAt(2, frontCol, PlantChoice::PEASHOOTER);
  TryCreatePlantAt(3, frontCol, PlantChoice::PEASHOOTER);
  TryCreatePlantAt(4, frontCol, PlantChoice::PEASHOOTER);
}

// 第三阶段：植物数量增加，部分行加入双层防线。
void GameWorld::GenerateStageThreePlants() {
  const int frontCol = m_redLineCol;
  const int sunCol = m_redLineCol - 1;

  for (int row = 0; row < GAME_ROWS; ++row) {
    TryCreatePlantAt(row, sunCol, PlantChoice::SUNFLOWER);
  }

  TryCreatePlantAt(0, frontCol, PlantChoice::PEASHOOTER);
  TryCreatePlantAt(0, sunCol - 1, PlantChoice::PEASHOOTER);

  TryCreatePlantAt(1, frontCol, PlantChoice::WALLNUT);
  TryCreatePlantAt(1, sunCol - 1, PlantChoice::PEASHOOTER);

  TryCreatePlantAt(2, frontCol, PlantChoice::PEASHOOTER);
  TryCreatePlantAt(2, sunCol - 1, PlantChoice::WALLNUT);

  TryCreatePlantAt(3, frontCol, PlantChoice::WALLNUT);
  TryCreatePlantAt(3, sunCol - 1, PlantChoice::PEASHOOTER);

  TryCreatePlantAt(4, frontCol, PlantChoice::PEASHOOTER);
  TryCreatePlantAt(4, sunCol - 1, PlantChoice::PEASHOOTER);
}

// 第四阶段：加入双发射手和更多坚果，防线更强。
void GameWorld::GenerateStageFourPlants() {
  const int frontCol = m_redLineCol;
  const int sunCol = m_redLineCol - 1;

  for (int row = 0; row < GAME_ROWS; ++row) {
    TryCreatePlantAt(row, sunCol, PlantChoice::SUNFLOWER);
  }

  TryCreatePlantAt(0, frontCol, PlantChoice::PEASHOOTER);
  TryCreatePlantAt(0, sunCol - 2, PlantChoice::WALLNUT);

  TryCreatePlantAt(1, frontCol, PlantChoice::WALLNUT);
  TryCreatePlantAt(1, sunCol - 1, PlantChoice::PEASHOOTER);

  TryCreatePlantAt(2, frontCol, PlantChoice::REPEATER);
  TryCreatePlantAt(2, sunCol - 2, PlantChoice::PEASHOOTER);

  TryCreatePlantAt(3, frontCol, PlantChoice::WALLNUT);
  TryCreatePlantAt(3, sunCol - 1, PlantChoice::REPEATER);

  TryCreatePlantAt(4, frontCol, PlantChoice::PEASHOOTER);
  TryCreatePlantAt(4, sunCol - 1, PlantChoice::WALLNUT);
}

// 最终阶段：防线最密集，综合使用向日葵、豌豆射手、双发射手和坚果。
void GameWorld::GenerateFinalStagePlants() {
  const int frontCol = m_redLineCol;
  const int sunCol = m_redLineCol - 1;

  for (int row = 0; row < GAME_ROWS; ++row) {
    TryCreatePlantAt(row, sunCol, PlantChoice::SUNFLOWER);
  }

  TryCreatePlantAt(0, frontCol, PlantChoice::WALLNUT);
  TryCreatePlantAt(0, sunCol - 2, PlantChoice::PEASHOOTER);
  TryCreatePlantAt(0, sunCol - 3, PlantChoice::WALLNUT);

  TryCreatePlantAt(1, frontCol, PlantChoice::REPEATER);
  TryCreatePlantAt(1, sunCol - 1, PlantChoice::WALLNUT);
  TryCreatePlantAt(1, sunCol - 3, PlantChoice::PEASHOOTER);

  TryCreatePlantAt(2, frontCol, PlantChoice::PEASHOOTER);
  TryCreatePlantAt(2, sunCol - 1, PlantChoice::REPEATER);
  TryCreatePlantAt(2, sunCol - 3, PlantChoice::WALLNUT);

  TryCreatePlantAt(3, frontCol, PlantChoice::REPEATER);
  TryCreatePlantAt(3, sunCol - 2, PlantChoice::WALLNUT);
  TryCreatePlantAt(3, sunCol - 3, PlantChoice::PEASHOOTER);

  TryCreatePlantAt(4, frontCol, PlantChoice::REPEATER);
  TryCreatePlantAt(4, sunCol - 1, PlantChoice::PEASHOOTER);
  TryCreatePlantAt(4, sunCol - 3, PlantChoice::WALLNUT);
}

// 尝试在指定格子创建一种植物。
// 如果越界或该格已有植物，就创建失败并返回 false。
bool GameWorld::TryCreatePlantAt(int row, int col, PlantChoice choice) {
  if (!IsInsideGrid(row, col) || m_plantAt[row][col] != nullptr) {
    return false;
  }

  // 先创建 unique_ptr<Plant>，具体类型由 choice 决定。
  std::unique_ptr<Plant> plant;
  switch (choice) {
  case PlantChoice::SUNFLOWER:
    plant = std::make_unique<Sunflower>(this, row, col);
    break;
  case PlantChoice::PEASHOOTER:
    plant = std::make_unique<Peashooter>(this, row, col);
    break;
  case PlantChoice::REPEATER:
    plant = std::make_unique<Repeater>(this, row, col);
    break;
  case PlantChoice::WALLNUT:
    plant = std::make_unique<WallNut>(this, row, col);
    break;
  }

  // rawPlant 是非拥有指针，只放进 m_plantAt 做快速索引。
  // 真正拥有植物对象的是 m_objects 中的 unique_ptr。
  Plant* rawPlant = plant.get();
  m_plantAt[row][col] = rawPlant;
  AddObject(std::move(plant));
  return true;
}

// 在指定行随机尝试放一个植物。
// 当前主流程使用的是手动配置布局；这个函数保留了随机生成接口。
bool GameWorld::CreateRandomPlantInRow(int row, PlantChoice choice) {
  int maxCol = m_redLineCol;
  if (maxCol >= GAME_COLS) {
    maxCol = GAME_COLS - 1;
  }
  if (maxCol < 1) {
    return false;
  }

  // 先随机尝试最多 20 次。
  for (int attempt = 0; attempt < 20; ++attempt) {
    const int col = randInt(1, maxCol);
    if (TryCreatePlantAt(row, col, choice)) {
      return true;
    }
  }

  // 如果随机尝试都失败，就从左到右找第一个可用格子，保证尽量生成成功。
  for (int col = 1; col <= maxCol; ++col) {
    if (TryCreatePlantAt(row, col, choice)) {
      return true;
    }
  }
  return false;
}

// 检查失败条件。
// 如果还有脑子活着，但玩家已经没有继续推进的手段，就失败。
void GameWorld::CheckGameOver() {
  if (HasAliveBrain() && !CanStillAct()) {
    m_status = LevelStatus::LOSING;
  }
}

// 判断玩家是否还能继续行动。
// 只要场上还有能推进的僵尸、还有可收集阳光、或者阳光足够买最便宜僵尸，就不失败。
bool GameWorld::CanStillAct() const {
  if (HasUsefulZombie() || HasCollectableSun()) {
    return true;
  }
  return m_sun >= 50;
}

// 判断场上是否还有可收集阳光。
// m_objects 和 m_newObjects 都要检查，因为阳光可能刚在本帧被创建，还没移入主容器。
bool GameWorld::HasCollectableSun() const {
  for (const auto& object : m_objects) {
    if (!object->IsDead() && object->GetCategory() == ObjectCategory::SUN) {
      return true;
    }
  }
  for (const auto& object : m_newObjects) {
    if (!object->IsDead() && object->GetCategory() == ObjectCategory::SUN) {
      return true;
    }
  }
  return false;
}

// 判断场上是否还有能继续推进的僵尸。
// 如果还有活着且在屏幕范围内的僵尸，就说明玩家仍可能吃掉脑子。
bool GameWorld::HasUsefulZombie() const {
  for (const auto& object : m_objects) {
    const Zombie* zombie = object->AsZombie();
    if (zombie != nullptr && !zombie->IsDead() && zombie->GetX() > 0 &&
        zombie->GetX() < WINDOW_WIDTH) {
      return true;
    }
  }
  for (const auto& object : m_newObjects) {
    const Zombie* zombie = object->AsZombie();
    if (zombie != nullptr && !zombie->IsDead() && zombie->GetX() > 0 &&
        zombie->GetX() < WINDOW_WIDTH) {
      return true;
    }
  }
  return false;
}

// 判断是否还有任意一行脑子活着。
bool GameWorld::HasAliveBrain() const {
  for (int row = 0; row < GAME_ROWS; ++row) {
    if (m_brainAlive[row]) {
      return true;
    }
  }
  return false;
}

// 判断某张卡牌当前是否可用。
// 条件：下标合法、没有冷却、阳光足够。
bool GameWorld::CanUseCard(int cardIndex) const {
  if (cardIndex < 0 || cardIndex >= CARD_COUNT) {
    return false;
  }
  return m_cards[cardIndex].currentCooldown == 0 &&
         m_sun >= m_cards[cardIndex].cost;
}

// 判断某个格子是否可以投放僵尸。
// 条件：格子在草坪内、在红线右侧、附近没有已有僵尸。
bool GameWorld::CanPlaceZombieAt(int row, int col) const {
  if (!IsInsideGrid(row, col)) {
    return false;
  }
  // 红线左侧或太靠近红线的位置不能投放普通行走僵尸。
  if (col < m_redLineCol + ZOMBIE_DEPLOYMENT_BUFFER_COLS) {
    return false;
  }

  // 防止同一行同一出生点生成多个完全重叠的僵尸。
  const int x = ColToX(col);
  for (const auto& object : m_objects) {
    const Zombie* zombie = object->AsZombie();
    if (zombie != nullptr && !zombie->IsDead() && zombie->GetRow() == row &&
        std::abs(zombie->GetX() - x) < 40) {
      return false;
    }
  }
  // 新对象队列也要检查，因为刚投放的僵尸可能还没有 Flush 到 m_objects。
  for (const auto& object : m_newObjects) {
    const Zombie* zombie = object->AsZombie();
    if (zombie != nullptr && !zombie->IsDead() && zombie->GetRow() == row &&
        std::abs(zombie->GetX() - x) < 40) {
      return false;
    }
  }
  return true;
}

// 判断行列是否在草坪范围内。
bool GameWorld::IsInsideGrid(int row, int col) const {
  return row >= 0 && row < GAME_ROWS && col >= 0 && col < GAME_COLS;
}

// 矩形碰撞判断。
// 这里的 x/y 表示中心点，w/h 表示宽高；两个矩形中心距离足够近时认为重叠。
bool GameWorld::IsRectOverlapping(int x1, int y1, int w1, int h1,
                                  int x2, int y2, int w2, int h2) const {
  return std::abs(x1 - x2) * 2 <= w1 + w2 &&
         std::abs(y1 - y2) * 2 <= h1 + h2;
}

// 当前选中卡牌文字。
std::string GameWorld::SelectedCardText() const {
  if (m_selectedCard < 0 || m_selectedCard >= CARD_COUNT) {
    return "Selected: none";
  }
  return "Selected: " + ZombieName(m_cards[m_selectedCard].type);
}
