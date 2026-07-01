#include "pvz/GameObject/SimpleObjects.hpp"

#include "pvz/GameObject/Zombie.hpp"
#include "pvz/GameWorld/GameWorld.hpp"

// 背景对象：放在窗口中央，大小覆盖整个窗口，图层为 BACKGROUND。
Background::Background(GameWorld* world)
    : GameObject(world, ImageID::BACKGROUND, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2,
                 LayerID::BACKGROUND, WINDOW_WIDTH, WINDOW_HEIGHT,
                 AnimID::NO_ANIMATION) {}

// 脑子对象：每行一个，位置在该行最左侧。
BrainTarget::BrainTarget(GameWorld* world, int row)
    : GameObject(world, ImageID::BRAIN_ICON, 35, GameWorld::RowToY(row),
                 LayerID::UI, 32, 31, AnimID::NO_ANIMATION),
      m_row(row) {}

// 脑子显示对象每帧检查 GameWorld 中对应行的脑子是否还活着。
// 真正的“脑子是否被吃掉”状态在 GameWorld 里统一保存。
void BrainTarget::Update() {
  if (!GetWorld()->IsBrainAlive(m_row)) {
    MarkDead();
  }
}

// 红线对象：x 坐标由当前阶段决定，y 坐标在草坪中间。
RedLine::RedLine(GameWorld* world)
    : GameObject(world, ImageID::RED_LINE, world->GetRedLineX(),
                 LAWN_GRID_CENTER_Y, LayerID::UI, 22, 502,
                 AnimID::NO_ANIMATION) {}

// 进度条对象：根据当前 stage 使用不同的进度图片。
ProgressMeter::ProgressMeter(GameWorld* world)
    : GameObject(world, world->GetProgressImage(), WINDOW_WIDTH / 2, 20,
                 LayerID::UI, 158, 24, AnimID::NO_ANIMATION) {}

// 僵尸卡牌按钮。
// cardIndex 对应 GameWorld::m_cards 中的下标；imageID 是卡牌图片。
CardButton::CardButton(GameWorld* world, int cardIndex, ImageID imageID)
    : GameObject(world, imageID, GameWorld::CardX(cardIndex), ZOMBIE_CARD_Y,
                 LayerID::UI, SEED_WIDTH, SEED_HEIGHT,
                 AnimID::NO_ANIMATION),
      m_cardIndex(cardIndex) {}

// 如果卡牌被选中，就把卡牌向上移动一点，显示“当前拿着这张卡”。
void CardButton::Update() {
  if (GetWorld()->IsCardSelected(m_cardIndex)) {
    MoveTo(GameWorld::CardX(m_cardIndex), ZOMBIE_CARD_Y + 10);
  } else {
    MoveTo(GameWorld::CardX(m_cardIndex), ZOMBIE_CARD_Y);
  }
}

// 点击卡牌时不直接扣阳光、不直接生成僵尸。
// 因为还不知道玩家要投放在哪一行哪一列，所以只通知 GameWorld 选中卡牌。
void CardButton::OnClick() {
  GetWorld()->TrySelectCard(m_cardIndex);
}

// 冷却遮罩初始放在屏幕外，避免不冷却时被玩家看到。
CooldownMask::CooldownMask(GameWorld* world, int cardIndex)
    : GameObject(world, ImageID::COOLDOWN_MASK, -100, -100,
                 LayerID::COOLDOWN_MASK, SEED_WIDTH, SEED_HEIGHT,
                 AnimID::NO_ANIMATION),
      m_cardIndex(cardIndex) {}

// 如果卡牌正在冷却，就把遮罩移动到卡牌上；否则移动到屏幕外隐藏。
void CooldownMask::Update() {
  if (GetWorld()->IsCardCooling(m_cardIndex)) {
    MoveTo(GameWorld::CardX(m_cardIndex), ZOMBIE_CARD_Y);
  } else {
    MoveTo(-100, -100);
  }
}

// 投放格对象：图片为 NONE，所以不可见，但它有位置和大小，可以接收点击。
// 每个草坪格子创建一个 DeploymentCell，点击后由 GameWorld 判断是否真的允许投放。
DeploymentCell::DeploymentCell(GameWorld* world, int row, int col)
    : GameObject(world, ImageID::NONE, GameWorld::ColToX(col),
                 GameWorld::RowToY(row), LayerID::UI, LAWN_GRID_WIDTH,
                 LAWN_GRID_HEIGHT - 10, AnimID::NO_ANIMATION),
      m_row(row),
      m_col(col) {}

// 点击投放格时，把行列传给 GameWorld。
// GameWorld 再检查是否选中卡牌、阳光是否足够、是否在红线右侧、出生点是否被占用。
void DeploymentCell::OnClick() {
  GetWorld()->TryDeployZombie(m_row, m_col);
}

// 阳光对象：显示为 SUN，使用 IDLE 动画，位于 SUN 图层。
SunPickup::SunPickup(GameWorld* world, int x, int y, int value)
    : GameObject(world, ImageID::SUN, x, y, LayerID::SUN, 80, 80,
                 AnimID::IDLE),
      m_value(value) {}

// 点击阳光：通知 GameWorld 增加阳光，然后标记自己死亡。
void SunPickup::OnClick() {
  GetWorld()->CollectSun(m_value);
  MarkDead();
}

// 阳光类别用于失败条件判断：如果场上还有可收集阳光，就不应该立刻失败。
ObjectCategory SunPickup::GetCategory() const {
  return ObjectCategory::SUN;
}

// 豌豆构造函数。
// 初始位置在植物格子右侧一点，xOffset 用于双发射手让两颗豌豆错开。
Pea::Pea(GameWorld* world, int row, int col, int xOffset)
    : GameObject(world, ImageID::PEA, GameWorld::ColToX(col) + 30 + xOffset,
                 GameWorld::RowToY(row) + 20, LayerID::PROJECTILES, 28, 28,
                 AnimID::NO_ANIMATION),
      m_row(row) {}

// 豌豆每帧向右移动，并检查是否命中僵尸。
void Pea::Update() {
  if (IsDead()) {
    return;
  }

  // 豌豆向右飞。
  MoveTo(GetX() + 8, GetY());

  // 飞出屏幕后标记死亡，等待 GameWorld 统一清理。
  if (GetX() >= WINDOW_WIDTH) {
    MarkDead();
    return;
  }

  // 查询同一行第一个被豌豆碰到的僵尸。
  Zombie* zombie = GetWorld()->FindZombieHitByPea(*this);
  if (zombie != nullptr) {
    // 命中后造成 24 点伤害，然后豌豆消失。
    zombie->TakeDamage(24);
    MarkDead();
  }
}

// 豌豆属于 PROJECTILE 类别。
ObjectCategory Pea::GetCategory() const {
  return ObjectCategory::PROJECTILE;
}

// 返回豌豆所在行，用于只检测同一行僵尸。
int Pea::GetRow() const {
  return m_row;
}
