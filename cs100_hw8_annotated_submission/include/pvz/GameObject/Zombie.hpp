#ifndef ZOMBIE_HPP__
#define ZOMBIE_HPP__

#include "pvz/GameObject/GameObject.hpp"

class Plant;

// Zombie 是所有普通行走类僵尸的共同基类。
// 它负责通用行为：从右往左移动、检测碰到植物、啃植物、吃脑子、受到豌豆伤害。
class Zombie : public GameObject {
public:
  // imageID 决定僵尸贴图，row/col 决定出生行列，hp 是生命值，animID 是初始动画。
  Zombie(GameWorld* world, ImageID imageID, int row, int col, int hp,
         AnimID animID = AnimID::WALK);
  ~Zombie() override = default;

  // 普通僵尸行为：如果碰到植物就啃，否则向左移动并尝试吃脑子。
  void Update() override;

  // 类型识别虚函数。GameWorld 通过 AsZombie() 判断对象是否为僵尸，不使用 dynamic_cast。
  ObjectCategory GetCategory() const override;
  Zombie* AsZombie() override;
  const Zombie* AsZombie() const override;

  // 基本查询和受伤接口。
  int GetRow() const;
  int GetHP() const;
  void TakeDamage(int damage);

protected:
  // 子类可复用的行为工具函数。
  void MoveLeft(int distance);
  Plant* GetTouchedPlant() const;
  bool TryEatBrain();
  void DamageTouchedPlant();

private:
  int m_row;      // 僵尸所在行。
  int m_hp;       // 僵尸当前血量。
  bool m_eating;  // 是否正在啃植物，用来控制 WALK/EAT 动画切换。
};

// 普通僵尸：完全使用 Zombie 的默认 Update 行为。
class RegularZombie : public Zombie {
public:
  RegularZombie(GameWorld* world, int row, int col);
};

// 铁桶僵尸：血量更高，血量低于阈值后铁桶消失，贴图换成普通僵尸。
class BucketZombie : public Zombie {
public:
  BucketZombie(GameWorld* world, int row, int col);

  void Update() override;

private:
  bool m_lostBucket; // 是否已经掉桶，避免重复切换贴图。
};

// 撑杆跳僵尸的状态机。
// RUN：拿杆快速跑；JUMP：跳过第一个植物；WALK：跳完后普通走路；EAT：碰到植物后啃食。
enum class PoleState {
  RUN,
  JUMP,
  WALK,
  EAT
};

// 撑杆跳僵尸：通过 m_state 控制不同阶段的行为和动画。
class PoleVaultingZombie : public Zombie {
public:
  PoleVaultingZombie(GameWorld* world, int row, int col);

  void Update() override;

private:
  // 四个状态分别对应四段逻辑，避免把所有 if/else 混在一个函数里。
  void UpdateRun();
  void UpdateJump();
  void UpdateWalk();
  void UpdateEat();

  PoleState m_state; // 当前状态。
  int m_jumpTicks;   // 跳跃剩余帧数，用来控制跳跃动画持续时间。
};

#endif // !ZOMBIE_HPP__
