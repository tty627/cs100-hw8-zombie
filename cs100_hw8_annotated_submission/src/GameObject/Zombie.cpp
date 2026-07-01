#include "pvz/GameObject/Zombie.hpp"

#include "pvz/GameObject/Plant.hpp"
#include "pvz/GameWorld/GameWorld.hpp"

// Zombie 构造函数：所有行走类僵尸共用。
// col/row 转成屏幕坐标后交给 GameObject/ObjectBase。
// LayerID::ZOMBIES 表示僵尸图层；20x80 是碰撞尺寸。
Zombie::Zombie(GameWorld* world, ImageID imageID, int row, int col, int hp,
               AnimID animID)
    : GameObject(world, imageID, GameWorld::ColToX(col), GameWorld::RowToY(row),
                 LayerID::ZOMBIES, 20, 80, animID),
      m_row(row),
      m_hp(hp),
      m_eating(false) {}

// 普通僵尸每帧行为。
// 这套逻辑也被 RegularZombie 直接复用，被 BucketZombie 先复用再追加掉桶逻辑。
void Zombie::Update() {
  if (IsDead()) {
    return;
  }

  // 先检查是否碰到植物。碰到就停止移动，播放啃食动画并伤害植物。
  Plant* plant = GetTouchedPlant();
  if (plant != nullptr) {
    if (!m_eating) {
      PlayAnimation(AnimID::EAT);
      m_eating = true;
    }
    DamageTouchedPlant();
    return;
  }

  // 如果上一帧正在啃食，但这一帧植物已经没了，就切回走路动画。
  if (m_eating) {
    PlayAnimation(AnimID::WALK);
    m_eating = false;
  }

  // 没碰到植物时向左移动，并尝试吃脑子。
  MoveLeft(1);
  TryEatBrain();
}

// 标识这个对象属于僵尸类别。
ObjectCategory Zombie::GetCategory() const {
  return ObjectCategory::ZOMBIE;
}

// 僵尸对象返回 this；非僵尸对象在 GameObject 基类中默认返回 nullptr。
Zombie* Zombie::AsZombie() {
  return this;
}

// const 版本。
const Zombie* Zombie::AsZombie() const {
  return this;
}

// 返回僵尸所在行。
int Zombie::GetRow() const {
  return m_row;
}

// 返回僵尸当前 HP。
int Zombie::GetHP() const {
  return m_hp;
}

// 僵尸受到豌豆伤害。
// HP 到 0 后只标记死亡，之后由 GameWorld 统一清理。
void Zombie::TakeDamage(int damage) {
  if (IsDead()) {
    return;
  }
  m_hp -= damage;
  if (m_hp <= 0) {
    m_hp = 0;
    MarkDead();
  }
}

// 向左移动 distance 个像素。
// 如果僵尸走出屏幕左侧还没有吃到脑子，就标记死亡，避免对象永远留在场上。
void Zombie::MoveLeft(int distance) {
  MoveTo(GetX() - distance, GetY());
  if (GetX() < 0) {
    MarkDead();
  }
}

// 查询当前僵尸是否碰到了植物。
// 碰撞判断不在 Zombie 内部遍历对象，而是交给 GameWorld，因为 GameWorld 管理所有对象。
Plant* Zombie::GetTouchedPlant() const {
  return GetWorld()->FindPlantCollidingWithZombie(*this);
}

// 尝试吃掉本行脑子。
// 如果 GameWorld 判断和脑子发生碰撞，就把该行脑子置为已吃掉，并让僵尸死亡。
bool Zombie::TryEatBrain() {
  if (GetWorld()->TryEatBrain(GetRow(), GetX(), GetY(), GetWidth(), GetHeight())) {
    MarkDead();
    return true;
  }
  return false;
}

// 对当前接触到的植物造成伤害。
// 这里再次查询植物，是为了拿到植物所在 row/col，然后让 GameWorld 统一扣血。
void Zombie::DamageTouchedPlant() {
  Plant* plant = GetTouchedPlant();
  if (plant != nullptr) {
    GetWorld()->DamagePlantAt(plant->GetRow(), plant->GetCol(), 4);
  }
}

// 普通僵尸：HP 260，使用普通僵尸贴图。
RegularZombie::RegularZombie(GameWorld* world, int row, int col)
    : Zombie(world, ImageID::REGULAR_ZOMBIE, row, col, 260) {}

// 铁桶僵尸：HP 1450，比普通僵尸高；初始贴图是铁桶僵尸。
BucketZombie::BucketZombie(GameWorld* world, int row, int col)
    : Zombie(world, ImageID::BUCKET_HEAD_ZOMBIE, row, col, 1450),
      m_lostBucket(false) {}

// 铁桶僵尸每帧先执行普通僵尸逻辑，再检查是否掉桶。
void BucketZombie::Update() {
  Zombie::Update();
  // HP 低于 200 时认为桶被打掉，贴图换成普通僵尸。
  if (!IsDead() && !m_lostBucket && GetHP() <= 200) {
    ChangeImage(ImageID::REGULAR_ZOMBIE);
    m_lostBucket = true;
  }
}

// 撑杆跳僵尸：初始 HP 420，初始动画为 RUN，状态为 RUN。
PoleVaultingZombie::PoleVaultingZombie(GameWorld* world, int row, int col)
    : Zombie(world, ImageID::POLE_VAULTING_ZOMBIE, row, col, 420,
             AnimID::RUN),
      m_state(PoleState::RUN),
      m_jumpTicks(0) {}

// 撑杆跳僵尸用状态机更新。
// 好处是每个状态的逻辑清楚，避免一个 Update 里塞大量复杂判断。
void PoleVaultingZombie::Update() {
  if (IsDead()) {
    return;
  }

  switch (m_state) {
  case PoleState::RUN:
    UpdateRun();
    break;
  case PoleState::JUMP:
    UpdateJump();
    break;
  case PoleState::WALK:
    UpdateWalk();
    break;
  case PoleState::EAT:
    UpdateEat();
    break;
  }
}

// RUN 状态：拿杆快速跑。
// 如果前方检测到可以跳过的植物，就进入 JUMP 状态；否则每帧向左移动 2 像素。
void PoleVaultingZombie::UpdateRun() {
  if (GetWorld()->FindPlantForPoleJump(*this) != nullptr) {
    m_state = PoleState::JUMP;
    m_jumpTicks = 42;
    PlayAnimation(AnimID::JUMP);
    return;
  }
  MoveLeft(2);
  TryEatBrain();
}

// JUMP 状态：播放一段跳跃动画。
// 动画持续 m_jumpTicks 帧，到时间后直接向左移动一段距离，表示跳过植物。
void PoleVaultingZombie::UpdateJump() {
  --m_jumpTicks;
  if (m_jumpTicks <= 0) {
    MoveLeft(150);
    m_state = PoleState::WALK;
    PlayAnimation(AnimID::WALK);
    TryEatBrain();
  }
}

// WALK 状态：跳完后变成普通走路速度。
// 如果碰到植物，进入 EAT 状态；否则向左移动并尝试吃脑子。
void PoleVaultingZombie::UpdateWalk() {
  if (GetTouchedPlant() != nullptr) {
    m_state = PoleState::EAT;
    PlayAnimation(AnimID::EAT);
    return;
  }
  MoveLeft(1);
  TryEatBrain();
}

// EAT 状态：啃食植物。
// 如果植物已经被吃掉，就切回 WALK 状态继续前进。
void PoleVaultingZombie::UpdateEat() {
  if (GetTouchedPlant() == nullptr) {
    m_state = PoleState::WALK;
    PlayAnimation(AnimID::WALK);
    return;
  }
  DamageTouchedPlant();
}
