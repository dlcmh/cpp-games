#include "Demo/LaserDemo.hpp"
#include "Cockpit/CockpitState.hpp"
#include "Lasers/LaserTypes.hpp"
#include "Lasers/XWingCannon.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace laserlab {

LaserDemo::LaserDemo() {
    reset();
}

void LaserDemo::update(float deltaSeconds) {
    cockpit_.showCanopy = showCockpit_;
    cockpit_.showDashboard = showCockpit_;
    cockpit_.showHud = showCockpit_;
    if (deltaSeconds <= 0.0f || paused_) {
        return;
    }

    cockpit_.time += deltaSeconds;
    cockpit_.phaseTime += deltaSeconds;
    cockpit_.steer = std::sin(cockpit_.time * 0.38f) * 0.18f;
    cockpit_.firing = std::max(0.0f, cockpit_.firing - deltaSeconds * 5.5f);
    cockpit_.flash = flash_;
    flash_ = std::max(0.0f, flash_ - deltaSeconds * 2.0f);
}

void LaserDemo::fire() {
    manualBurstTime_ = cockpit_.time;
    cockpit_.firing = 1.0f;
}

void LaserDemo::toggleAuto() {
    autoFire_ = !autoFire_;
}

void LaserDemo::togglePause() {
    paused_ = !paused_;
}

void LaserDemo::toggleCockpit() {
    showCockpit_ = !showCockpit_;
}

void LaserDemo::reset() {
    cockpit_ = cppgames::cockpit::CockpitState{};
    cockpit_.glassTint = 0.08f;
    autoFire_ = true;
    paused_ = false;
    showCockpit_ = true;
    manualBurstTime_ = -1.0f;
    flash_ = 0.0f;
}

const cppgames::cockpit::CockpitState& LaserDemo::cockpitState() const noexcept {
    return cockpit_;
}

std::vector<cppgames::lasers::LaserBeam> LaserDemo::beams() const {
    cppgames::lasers::XWingCannonFrame frame{};
    frame.forward[2] = -1.0f;

    float manualAge = -1.0f;
    if (manualBurstTime_ >= 0.0f) {
        const float age = cockpit_.time - manualBurstTime_;
        if (age >= 0.0f && age <= 2.0f) {
            manualAge = age;
        }
    }
    cppgames::lasers::XWingCannonSettings settings{};
    settings.aimNdcX = -cockpit_.steer * cppgames::cockpit::kReticleSteerShift / 1.777f;
    settings.aimNdcY = cppgames::cockpit::kReticleCenterY;
    return cppgames::lasers::buildXWingCannonBeams(settings, frame, cockpit_.time, autoFire_,
                                                   manualAge);
}

std::string LaserDemo::windowTitle() const {
    char title[320];
    std::snprintf(title, sizeof(title), "X-Wing Laser Lab | %s | Cockpit %s",
                  paused_ ? "PAUSED" : (autoFire_ ? "AUTO" : "MANUAL"),
                  showCockpit_ ? "on" : "off");
    return title;
}

}  // namespace laserlab
