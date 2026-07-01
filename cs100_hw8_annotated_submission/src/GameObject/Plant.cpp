#include "pvz/GameObject/Plant.hpp"

#include "pvz/GameWorld/GameWorld.hpp"

// Plant 构造函数：所有植物共用。
// row/col 是逻辑格子坐标，GameWorld::ColToX/RowToY 把它转换为屏幕坐标。
// LayerID::PLANTS 表示植物图层；60x80 是植物碰撞和显示的大致尺寸。
Plant::Plant(GameWorld* world, ImageID imageID, int row, int col, int hp)
    : GameObject(world, imageID, GameWorld::ColToX(col), GameWorld::RowToY(row),
                 LayerID::PLANTS, 60, 80, AnimID::IDLE),
      m_row(row),
      m_col(col),
      m_hp(hp),
      m_maxHP(hp) {}

// 标识这个对象属于植物类别。
ObjectCategory Plant::GetCategory() const {
  return ObjectCategory::PLANT;
}

// 植物对象返回 this；非植物对象在 GameObject 基类中默认返回 nullptr。
Plant* Plant::AsPlant() {
  return this;
}

// const 版本，供 const 成员函数中使用。
const Plant* Plant::AsPlant() const {
  return this;
}

// 返回所在行。
int Plant::GetRow() const {
  return m_row;
}

// 返回所在列。
int Plant::GetCol() const {
  return m_col;
}

// 返回当前 HP。
int Plant::GetHP() const {
  return m_hp;
}

// 返回最大 HP。
int Plant::GetMaxHP() const {
  return m_maxHP;
}

// 植物受到伤害。
// 如果已经死亡，直接返回，避免重复扣血或重复触发死亡逻辑。
void Plant::TakeDamage(int damage) {
  if (IsDead()) {
    return;
  }
  m_hp -= damage;
  if (m_hp <= 0) {
    m_hp = 0;
    // 先标记死亡，稍后由 GameWorld::RemoveDeadObjects 统一删除。
    MarkDead();
    // 调用死亡钩子。向日葵会在这里掉落阳光。
    OnKilled();
  }
}

// 默认死亡时没有额外行为。
void Plant::OnKilled() {}

// 向日葵血量 340，贴图为 SUNFLOWER。
Sunflower::Sunflower(GameWorld* world, int row, int col)
    : Plant(world, ImageID::SUNFLOWER, row, col, 340) {}

// 向日葵被吃掉后掉落 6 个阳光。
// 每个阳光价值 SUN_VALUE，一共相当于 150 阳光。
void Sunflower::OnKilled() {
  const int baseX = GetX();
  const int baseY = GetY();
  for (int i = 0; i < 6; ++i) {
    // 让 6 个阳光稍微错开，避免完全重叠在一起。
    const int dx = (i % 3 - 1) * 24;
    const int dy = (i / 3) * 24;
    // 通过 GameWorld 创建阳光对象，而不是向日葵自己管理对象生命周期。
    GetWorld()->CreateSun(baseX + dx, baseY + dy, SUN_VALUE);
  }
}

// 会射击植物的构造函数。
// Peashooter 和 Repeater 都是 340 HP，冷却初始为 0，表示一开始可以攻击。
ShootingPlant::ShootingPlant(GameWorld* world, ImageID imageID, int row, int col)
    : Plant(world, imageID, row, col, 340), m_cooldown(0) {}

// 射击植物每帧更新。
void ShootingPlant::Update() {
  if (IsDead()) {
    return;
  }

  // 如果正在冷却，先减少冷却时间，本帧不攻击。
  if (m_cooldown > 0) {
    --m_cooldown;
    return;
  }

  // 只有同一行右侧存在僵尸时才发射。
  // 这个查询交给 GameWorld 做，因为只有 GameWorld 管理全部对象。
  if (!GetWorld()->HasZombieOnRight(GetRow(), GetCol())) {
    return;
  }

  // 根据具体植物类型决定发射数量。
  const int shootCount = GetShootCount();
  for (int i = 0; i < shootCount; ++i) {
    // Repeater 发两颗豌豆时，用 xOffset 让两颗稍微错开。
    GetWorld()->CreatePea(GetRow(), GetCol(), i * 18);
  }
  // 发射后进入冷却，避免每一帧都产生豌豆。
  m_cooldown = 32;
}

// 默认一次发射一颗豌豆。
int ShootingPlant::GetShootCount() const {
  return 1;
}

// 豌豆射手只需要指定贴图，其余射击逻辑继承 ShootingPlant。
Peashooter::Peashooter(GameWorld* world, int row, int col)
    : ShootingPlant(world, ImageID::PEASHOOTER, row, col) {}

// 双发射手只需要指定贴图，并重写发射数量。
Repeater::Repeater(GameWorld* world, int row, int col)
    : ShootingPlant(world, ImageID::REPEATER, row, col) {}

// 双发射手一次发射两颗豌豆。
int Repeater::GetShootCount() const {
  return 2;
}

// 坚果墙：血量 3600，比普通植物高很多。
WallNut::WallNut(GameWorld* world, int row, int col)
    : Plant(world, ImageID::WALLNUT, row, col, 3600), m_cracked(false) {}

// 坚果每帧检查是否需要切换破损贴图。
void WallNut::Update() {
  if (IsDead()) {
    return;
  }
  // 当 HP <= 最大 HP 的 1/3 时，切换到破损坚果图片。
  // 用 GetHP() * 3 <= GetMaxHP() 避免浮点数比较。
  if (!m_cracked && GetHP() * 3 <= GetMaxHP()) {
    ChangeImage(ImageID::WALLNUT_CRACKED);
    m_cracked = true;
  }
}
