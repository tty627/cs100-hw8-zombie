#ifndef GAMEOBJECT_HPP__
#define GAMEOBJECT_HPP__

#include "pvz/Framework/ObjectBase.hpp"

// 前向声明：这里只需要使用这些类的指针，不需要知道类的完整内容。
// 好处是减少头文件之间的相互 include，降低编译依赖。
class GameWorld;
class Plant;
class Zombie;

// 对游戏对象做“粗分类”。
// 注意这不是为了判断某个对象的精确类型，而是为了回答“它是不是植物/僵尸/阳光/UI”等大类问题。
// 评分标准要求不要乱用 dynamic_cast/typeid，所以这里用 enum class + 虚函数表达类别。
enum class ObjectCategory {
  OTHER,
  PLANT,
  ZOMBIE,
  PROJECTILE,
  SUN,
  UI
};

// 僵尸卡牌和生成僵尸时使用的僵尸类型。
// enum class 是限定作用域枚举，使用时要写 ZombieType::REGULAR，避免和别的名字冲突。
enum class ZombieType {
  REGULAR,
  BUCKET,
  POLE_VAULTING
};

// 简单的位置结构，目前主要用于表达“行列坐标”的语义。
// row/col 默认 -1 表示无效位置。
struct Position {
  int row = -1;
  int col = -1;
};

// 当前实现有三张僵尸卡：普通僵尸、铁桶僵尸、撑杆跳僵尸。
const int CARD_COUNT = 3;

// 每一张僵尸卡的状态。
// GameWorld 用一个数组保存所有卡牌状态，卡牌按钮只负责点击时通知 GameWorld。
struct CardState {
  ZombieType type = ZombieType::REGULAR; // 这张卡会生成哪一种僵尸。
  int cost = 0;                          // 投放该僵尸需要消耗多少阳光。
  int cooldown = 0;                      // 投放成功后进入冷却的总时长。
  int currentCooldown = 0;               // 当前剩余冷却时间；为 0 表示可用。
  bool selected = false;                 // 玩家当前是否选中了这张卡。
};

// 所有游戏对象的共同基类。
// 背景、脑子、红线、卡牌、植物、僵尸、豌豆、阳光都继承 GameObject。
// 这样 GameWorld 可以用 vector<unique_ptr<GameObject>> 统一保存所有对象。
class GameObject : public ObjectBase {
public:
  // 构造函数把“所有对象都有的图像、坐标、图层、尺寸、动画”等参数传给 ObjectBase。
  // world 是指向 GameWorld 的非拥有指针，对象通过它请求 GameWorld 创建豌豆、扣血、收集阳光等。
  GameObject(GameWorld* world, ImageID imageID, int x, int y, LayerID layer,
             int width, int height, AnimID animID);
  ~GameObject() override = default;

  // 每一帧由框架间接调用。基类默认什么都不做，具体对象按需要重写。
  // 例如僵尸重写 Update 来移动和啃植物，豌豆重写 Update 来飞行和碰撞。
  void Update() override;

  // 鼠标点击对象时由框架调用。基类默认什么都不做，卡牌/阳光/投放格会重写。
  void OnClick() override;

  // 统一的“延迟删除”机制。
  // 对象不直接 delete 自己，而是 MarkDead() 标记死亡，之后 GameWorld 统一清理。
  bool IsDead() const;
  void MarkDead();

  // 类型相关的虚函数。
  // 基类默认返回 OTHER/nullptr；Plant/Zombie 子类会重写这些函数。
  // 这样 GameWorld 可以判断“是否为植物/僵尸”，又不需要 dynamic_cast。
  virtual ObjectCategory GetCategory() const;
  virtual Plant* AsPlant();
  virtual Zombie* AsZombie();
  virtual const Plant* AsPlant() const;
  virtual const Zombie* AsZombie() const;

protected:
  // 提供给子类访问 GameWorld 的接口。
  // 例如 ShootingPlant 通过 GetWorld()->CreatePea(...) 创建豌豆。
  GameWorld* GetWorld() const;

private:
  GameWorld* m_world; // 非拥有指针：对象知道自己属于哪个 GameWorld，但不负责释放 GameWorld。
  bool m_dead;        // 是否已经死亡；死亡对象会在 GameWorld::RemoveDeadObjects 中被清理。

};

#endif // !GAMEOBJECT_HPP__
