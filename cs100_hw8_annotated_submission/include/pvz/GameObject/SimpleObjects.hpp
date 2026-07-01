#ifndef SIMPLEOBJECTS_HPP__
#define SIMPLEOBJECTS_HPP__

#include "pvz/GameObject/GameObject.hpp"

// 背景对象：只负责显示背景图，不需要 Update/OnClick。
class Background : public GameObject {
public:
  explicit Background(GameWorld* world);
};

// 脑子目标：每一行最左侧一个。
// 它本身只负责显示；是否被吃掉的状态保存在 GameWorld::m_brainAlive 中。
class BrainTarget : public GameObject {
public:
  BrainTarget(GameWorld* world, int row);

  // 每帧检查 GameWorld 中本行脑子是否还活着；如果已经被吃掉，就标记自己死亡。
  void Update() override;

private:
  int m_row; // 这个脑子属于哪一行。
};

// 推进红线：表示当前阶段植物防线和可投放区域的边界。
class RedLine : public GameObject {
public:
  explicit RedLine(GameWorld* world);
};

// 关卡进度条：用不同图片表示当前阶段。
class ProgressMeter : public GameObject {
public:
  explicit ProgressMeter(GameWorld* world);
};

// 僵尸卡牌按钮。
// 点击后不是立刻生成僵尸，而是让 GameWorld 记录“当前选中了哪张卡”。
class CardButton : public GameObject {
public:
  CardButton(GameWorld* world, int cardIndex, ImageID imageID);

  // 每帧根据是否被选中调整位置，给玩家一个视觉反馈。
  void Update() override;

  // 点击卡牌时通知 GameWorld 尝试选中这张卡。
  void OnClick() override;

private:
  int m_cardIndex; // 这张按钮对应 m_cards 数组里的哪个下标。
};

// 冷却遮罩：卡牌冷却时显示在卡牌上，不冷却时移动到屏幕外隐藏。
class CooldownMask : public GameObject {
public:
  CooldownMask(GameWorld* world, int cardIndex);

  void Update() override;

private:
  int m_cardIndex; // 对应哪张卡牌。
};

// 僵尸投放格。
// 它没有图片，但有点击区域；玩家点格子时由它通知 GameWorld 尝试投放僵尸。
class DeploymentCell : public GameObject {
public:
  DeploymentCell(GameWorld* world, int row, int col);

  void OnClick() override;

private:
  int m_row; // 投放格所在行。
  int m_col; // 投放格所在列。
};

// 可点击阳光。
// 向日葵死亡后生成，玩家点击后增加阳光并消失。
class SunPickup : public GameObject {
public:
  SunPickup(GameWorld* world, int x, int y, int value);

  void OnClick() override;
  ObjectCategory GetCategory() const override;

private:
  int m_value; // 点击后增加多少阳光。
};

// 豌豆子弹。
// 由 Peashooter/Repeater 创建，向右移动，碰到第一个僵尸后造成伤害并消失。
class Pea : public GameObject {
public:
  Pea(GameWorld* world, int row, int col, int xOffset);

  void Update() override;
  ObjectCategory GetCategory() const override;
  int GetRow() const;

private:
  int m_row; // 豌豆所在行，只能打同一行的僵尸。
};

#endif // !SIMPLEOBJECTS_HPP__
