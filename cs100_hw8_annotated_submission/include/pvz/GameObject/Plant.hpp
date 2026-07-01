#ifndef PLANT_HPP__
#define PLANT_HPP__

#include "pvz/GameObject/GameObject.hpp"

// Plant 是所有植物的共同基类。
// 它负责保存植物共有的数据：所在行列、当前 HP、最大 HP。
// Sunflower、Peashooter、Repeater、WallNut 都继承它。
class Plant : public GameObject {
public:
  // imageID 决定植物贴图，row/col 决定植物在草坪格子中的位置，hp 是植物生命值。
  Plant(GameWorld* world, ImageID imageID, int row, int col, int hp);
  ~Plant() override = default;

  // 重写 GameObject 的类型识别函数。
  // GameWorld 可以通过 AsPlant() 判断一个 GameObject 是否为植物，而不用 dynamic_cast。
  ObjectCategory GetCategory() const override;
  Plant* AsPlant() override;
  const Plant* AsPlant() const override;

  // 基本 getter。很多碰撞和攻击逻辑需要知道植物所在行列和生命值。
  int GetRow() const;
  int GetCol() const;
  int GetHP() const;
  int GetMaxHP() const;

  // 僵尸啃植物时调用。
  // HP 扣到 0 后标记死亡，并触发 OnKilled() 钩子函数。
  void TakeDamage(int damage);

protected:
  // 植物死亡时的扩展行为。
  // 普通植物默认什么都不做，向日葵会重写它来掉落阳光。
  virtual void OnKilled();

private:
  int m_row;   // 植物所在行。
  int m_col;   // 植物所在列。
  int m_hp;    // 当前血量。
  int m_maxHP; // 最大血量，用于坚果判断是否进入破损状态。
};

// 向日葵：I, Zombie 模式里不会主动产阳光，而是在被吃掉时掉落阳光。
class Sunflower : public Plant {
public:
  Sunflower(GameWorld* world, int row, int col);

protected:
  // 向日葵死亡时生成 6 个可点击阳光，每个价值 SUN_VALUE。
  void OnKilled() override;
};

// ShootingPlant 是“会射击的植物”的共同基类。
// Peashooter 和 Repeater 的发射检测、冷却逻辑相同，所以放在这里复用。
class ShootingPlant : public Plant {
public:
  ShootingPlant(GameWorld* world, ImageID imageID, int row, int col);

  // 每帧检查右侧是否有僵尸；如果有且冷却结束，就创建豌豆。
  void Update() override;

protected:
  // 每次攻击发射几颗豌豆。
  // Peashooter 使用默认值 1；Repeater 重写为 2。
  virtual int GetShootCount() const;

private:
  int m_cooldown; // 当前射击冷却，防止每一帧都发射。
};

// 普通豌豆射手：一次发射一颗豌豆。
class Peashooter : public ShootingPlant {
public:
  Peashooter(GameWorld* world, int row, int col);
};

// 双发射手：继承 ShootingPlant，只重写 GetShootCount()，避免复制整套射击逻辑。
class Repeater : public ShootingPlant {
public:
  Repeater(GameWorld* world, int row, int col);

protected:
  int GetShootCount() const override;
};

// 坚果墙：高血量植物，被啃到一定程度后换成破损贴图。
class WallNut : public Plant {
public:
  WallNut(GameWorld* world, int row, int col);

  // 每帧检查血量是否低于阈值，必要时更换贴图。
  void Update() override;

private:
  bool m_cracked; // 是否已经切换到破损贴图，避免反复 ChangeImage。
};

#endif // !PLANT_HPP__
