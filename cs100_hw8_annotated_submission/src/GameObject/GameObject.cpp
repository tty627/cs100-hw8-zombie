#include "pvz/GameObject/GameObject.hpp"

// GameObject 构造函数。
// 这里先调用父类 ObjectBase 的构造函数，设置图片、坐标、图层、大小和动画；
// 然后保存 GameWorld 指针，并把死亡状态初始化为 false。
GameObject::GameObject(GameWorld* world, ImageID imageID, int x, int y,
                       LayerID layer, int width, int height, AnimID animID)
    : ObjectBase(imageID, x, y, layer, width, height, animID),
      m_world(world),
      m_dead(false) {}

// 基类默认每帧不做任何事情。
// 这样像背景这类静态对象可以不重写 Update。
void GameObject::Update() {}

// 基类默认不响应点击。
// 只有卡牌、投放格、阳光等需要交互的对象才重写 OnClick。
void GameObject::OnClick() {}

// 返回对象是否已经被标记死亡。
bool GameObject::IsDead() const {
  return m_dead;
}

// 标记死亡，但不在这里释放内存。
// 真正删除发生在 GameWorld::RemoveDeadObjects()，这样可以避免对象在 Update 过程中自删导致容器出问题。
void GameObject::MarkDead() {
  m_dead = true;
}

// 默认类别是 OTHER。
ObjectCategory GameObject::GetCategory() const {
  return ObjectCategory::OTHER;
}

// 默认不是 Plant，所以返回 nullptr。
Plant* GameObject::AsPlant() {
  return nullptr;
}

// 默认不是 Zombie，所以返回 nullptr。
Zombie* GameObject::AsZombie() {
  return nullptr;
}

// const 版本：在 const 函数里也能判断对象是否为 Plant。
const Plant* GameObject::AsPlant() const {
  return nullptr;
}

// const 版本：在 const 函数里也能判断对象是否为 Zombie。
const Zombie* GameObject::AsZombie() const {
  return nullptr;
}

// 返回所属的 GameWorld。
// 子类用它和整个游戏世界交互，但这个指针本身不拥有 GameWorld。
GameWorld* GameObject::GetWorld() const {
  return m_world;
}
