#include "Demo/LaserDemo.hpp"
#include "Lasers/LaserTypes.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace laserlab {
namespace {

float boltT(float travel) {
    return std::clamp(travel / 120.0f, 0.0f, 1.0f);
}

}  // namespace

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

    if (autoFire_) {
        autoFireTimer_ += deltaSeconds;
        if (autoFireTimer_ > 0.62f) {
            spawnVolley();
            autoFireTimer_ = 0.0f;
        }
    }

    for (LaserBolt& bolt : bolts_) {
        bolt.travel += bolt.speed * deltaSeconds;
    }
    bolts_.erase(std::remove_if(bolts_.begin(), bolts_.end(),
                                [](const LaserBolt& bolt) { return bolt.travel > 125.0f; }),
                 bolts_.end());
}

void LaserDemo::fire() {
    spawnVolley();
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
    bolts_.clear();
    cockpit_ = cppgames::cockpit::CockpitState{};
    cockpit_.glassTint = 0.08f;
    autoFire_ = true;
    paused_ = false;
    showCockpit_ = true;
    autoFireTimer_ = 0.0f;
    flash_ = 0.0f;
}

const cppgames::cockpit::CockpitState& LaserDemo::cockpitState() const noexcept {
    return cockpit_;
}

std::vector<cppgames::lasers::LaserBeam> LaserDemo::beams() const {
    std::vector<cppgames::lasers::LaserBeam> result;
    result.reserve(bolts_.size());

    for (const LaserBolt& bolt : bolts_) {
        const float t0 = boltT(bolt.travel);
        const float t1 = boltT(bolt.travel + bolt.length);
        const float x0 = static_cast<float>(bolt.side) * (1.25f - 0.95f * t0);
        const float x1 = static_cast<float>(bolt.side) * (1.25f - 0.95f * t1);
        const float y0 = -0.55f + 0.90f * t0;
        const float y1 = -0.55f + 0.90f * t1;

        cppgames::lasers::LaserBeam beam{};
        beam.start[0] = x0;
        beam.start[1] = y0;
        beam.start[2] = -4.0f - bolt.travel;
        beam.end[0] = x1;
        beam.end[1] = y1;
        beam.end[2] = -4.0f - bolt.travel - bolt.length;
        beam.width = 0.18f;
        beam.color[0] = 1.0f;
        beam.color[1] = 0.13f;
        beam.color[2] = 0.035f;
        beam.intensity = 1.2f;
        beam.style = 0;
        beam.seed = bolt.seed;
        result.push_back(beam);
    }
    return result;
}

std::string LaserDemo::windowTitle() const {
    char title[320];
    std::snprintf(title, sizeof(title), "X-Wing Laser Lab | %s | bolts %zu | Cockpit %s",
                  paused_ ? "PAUSED" : (autoFire_ ? "AUTO" : "MANUAL"), bolts_.size(),
                  showCockpit_ ? "on" : "off");
    return title;
}

void LaserDemo::spawnVolley() {
    if (bolts_.size() > 50) {
        return;
    }

    for (int side = -1; side <= 1; side += 2) {
        LaserBolt bolt{};
        bolt.side = side;
        bolt.speed = 30.0f;
        bolt.length = 8.0f;
        bolt.seed = cockpit_.time * 0.73f + static_cast<float>(side) * 11.0f + bolt.travel;
        bolts_.push_back(bolt);
    }
}

}  // namespace laserlab
