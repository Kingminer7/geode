#include <Geode/ui/SceneManager.hpp>

using namespace geode::prelude;

#ifdef GEODE_IS_WINDOWS
#include <Geode/modify/AppDelegate.hpp>
#else
#include <Geode/modify/AchievementNotifier.hpp>
#endif

namespace geode {

#ifdef GEODE_IS_WINDOWS
struct SceneSwitch : Modify<SceneSwitch, AppDelegate> {
    GEODE_FORWARD_COMPAT_DISABLE_HOOKS("persist disabled")
    void willSwitchToScene(CCScene* scene) {
        AppDelegate::willSwitchToScene(scene);
        SceneManager::get()->willSwitchToScene(scene);
    }
};

#else
struct SceneSwitch : Modify<SceneSwitch, AchievementNotifier> {
    GEODE_FORWARD_COMPAT_DISABLE_HOOKS("persist disabled")
    void willSwitchToScene(CCScene* scene) {
        AchievementNotifier::willSwitchToScene(scene);
        SceneManager::get()->willSwitchToScene(scene);
    }
};

#endif

}