#include "engine/SceneManager.h"

#include <utility>

namespace engine {

void SceneManager::Push(std::unique_ptr<Scene> scene) {
    pending_.push_back({Op::Push, std::move(scene)});
}

void SceneManager::Pop() {
    pending_.push_back({Op::Pop, nullptr});
}

void SceneManager::Replace(std::unique_ptr<Scene> scene) {
    pending_.push_back({Op::Replace, std::move(scene)});
}

void SceneManager::Clear() {
    pending_.push_back({Op::Clear, nullptr});
}

void SceneManager::ApplyPending() {
    // Take the queue first: OnEnter/OnExit may queue more changes, which apply next frame.
    std::vector<Change> changes = std::move(pending_);
    pending_.clear();

    for (Change& change : changes) {
        switch (change.op) {
            case Op::Push:
                stack_.push_back(std::move(change.scene));
                stack_.back()->OnEnter();
                break;
            case Op::Pop:
                PopTop();
                break;
            case Op::Replace:
                PopTop();
                stack_.push_back(std::move(change.scene));
                stack_.back()->OnEnter();
                break;
            case Op::Clear:
                while (!stack_.empty()) {
                    PopTop();
                }
                break;
        }
    }
}

void SceneManager::Update(float dt) {
    if (!stack_.empty()) {
        stack_.back()->Update(dt);
    }
}

void SceneManager::FixedUpdate(float dt) {
    if (!stack_.empty()) {
        stack_.back()->FixedUpdate(dt);
    }
}

void SceneManager::Draw(float alpha) const {
    for (const auto& scene : stack_) {
        scene->Draw(alpha);
    }
}

void SceneManager::PopTop() {
    if (stack_.empty()) {
        return;
    }
    stack_.back()->OnExit();
    stack_.pop_back();
}

}  // namespace engine
                    