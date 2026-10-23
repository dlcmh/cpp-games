#include "Core/Track.hpp"
#include "Core/Math.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

namespace rollercoaster {
namespace {

constexpr int kStepsPerSegment = 32;
constexpr int kSampleCount = 700;

// Centripetal Catmull-Rom. Uniform Catmull-Rom can overshoot and make visible
// kinks; the centripetal parameterisation keeps the curve much better behaved.
Vec3 catmullRom(Vec3 p0, Vec3 p1, Vec3 p2, Vec3 p3, float t) {
    const auto knot = [](float previous, Vec3 a, Vec3 b) {
        return previous + std::sqrt(std::max(0.000001f, length(b - a)));
    };

    const float t0 = 0.0f;
    const float t1 = knot(t0, p0, p1);
    const float t2 = knot(t1, p1, p2);
    const float t3 = knot(t2, p2, p3);
    const float time = t1 + (t2 - t1) * t;

    const auto lerp = [](Vec3 a, Vec3 b, float alpha) { return a * (1.0f - alpha) + b * alpha; };

    const Vec3 a1 = lerp(p0, p1, (time - t0) / (t1 - t0));
    const Vec3 a2 = lerp(p1, p2, (time - t1) / (t2 - t1));
    const Vec3 a3 = lerp(p2, p3, (time - t2) / (t3 - t2));
    const Vec3 b1 = lerp(a1, a2, (time - t0) / (t2 - t0));
    const Vec3 b2 = lerp(a2, a3, (time - t1) / (t3 - t1));
    return lerp(b1, b2, (time - t1) / (t2 - t1));
}

float bankFor(const Track& track, std::size_t index) {
    const std::size_t count = track.samples.size();
    const PathSample& previous = track.samples[(index + count - 1) % count];
    const PathSample& current = track.samples[index];
    const float turn = dot(cross(previous.tangent, current.tangent), Vec3{0.0f, 1.0f, 0.0f});
    return std::clamp(turn * 55.0f, -0.14f, 0.14f);
}

}  // namespace

Track buildTrack() {
    // The route is authored as a closed loop of peaks, turns, and valleys.
    const std::array<Vec3, 10> controlPoints = {{
        {0.0f, 20.0f, 0.0f},
        {0.0f, 16.0f, 26.0f},
        {20.0f, 10.0f, 38.0f},
        {36.0f, 5.0f, 24.0f},
        {32.0f, 2.0f, -2.0f},
        {16.0f, 7.0f, -22.0f},
        {-4.0f, 14.0f, -28.0f},
        {-22.0f, 8.0f, -14.0f},
        {-26.0f, 4.0f, 8.0f},
        {-14.0f, 8.0f, 24.0f},
    }};

    const int count = static_cast<int>(controlPoints.size());
    std::vector<Vec3> dense;
    for (int segment = 0; segment < count; ++segment) {
        const Vec3 p0 = controlPoints[(segment + count - 1) % count];
        const Vec3 p1 = controlPoints[segment];
        const Vec3 p2 = controlPoints[(segment + 1) % count];
        const Vec3 p3 = controlPoints[(segment + 2) % count];
        for (int step = 0; step < kStepsPerSegment; ++step) {
            dense.push_back(catmullRom(
                p0, p1, p2, p3, static_cast<float>(step) / static_cast<float>(kStepsPerSegment)));
        }
    }

    std::vector<float> cumulative(dense.size() + 1, 0.0f);
    for (std::size_t i = 1; i <= dense.size(); ++i) {
        cumulative[i] = cumulative[i - 1] + length(dense[i % dense.size()] - dense[i - 1]);
    }
    const float total = cumulative.back();

    std::vector<Vec3> points;
    points.reserve(kSampleCount);
    for (int i = 0; i < kSampleCount; ++i) {
        const float target = total * static_cast<float>(i) / static_cast<float>(kSampleCount);
        const auto upper = std::upper_bound(cumulative.begin(), cumulative.end(), target);
        std::size_t segment = upper == cumulative.begin()
                                  ? 0
                                  : static_cast<std::size_t>(upper - cumulative.begin() - 1);
        segment = std::min(segment, dense.size() - 1);
        const float segmentLength = cumulative[segment + 1] - cumulative[segment];
        const float t =
            segmentLength > 0.0001f ? (target - cumulative[segment]) / segmentLength : 0.0f;
        points.push_back(dense[segment] +
                         (dense[(segment + 1) % dense.size()] - dense[segment]) * t);
    }

    Track track;
    track.length = total;
    track.startHeight = points.front().y;
    track.samples.resize(points.size());

    for (std::size_t i = 0; i < points.size(); ++i) {
        const std::size_t countSamples = points.size();
        const Vec3 tangent = normalize(points[(i + 1) % countSamples] -
                                       points[(i + countSamples - 1) % countSamples]);
        Vec3 right = normalize(cross(tangent, Vec3{0.0f, 1.0f, 0.0f}));
        const Vec3 up = normalize(cross(right, tangent));
        track.samples[i] = {points[i], tangent, right, up};
    }

    // Smooth the bank signal before applying it so small numerical changes in
    // tangent do not turn into camera roll jitter.
    std::vector<float> bank(track.samples.size(), 0.0f);
    for (std::size_t i = 0; i < track.samples.size(); ++i) {
        bank[i] = bankFor(track, i);
    }
    for (int pass = 0; pass < 6; ++pass) {
        std::vector<float> smoothed = bank;
        for (std::size_t i = 0; i < bank.size(); ++i) {
            const std::size_t previous = (i + bank.size() - 1) % bank.size();
            const std::size_t next = (i + 1) % bank.size();
            smoothed[i] = (bank[previous] + bank[i] * 2.0f + bank[next]) * 0.25f;
        }
        bank = std::move(smoothed);
    }

    for (std::size_t i = 0; i < track.samples.size(); ++i) {
        PathSample& sample = track.samples[i];
        const float cosine = std::cos(bank[i]);
        const float sine = std::sin(bank[i]);
        sample.right =
            normalize(sample.right * cosine + cross(sample.tangent, sample.right) * sine);
        sample.up = normalize(cross(sample.right, sample.tangent));
    }
    return track;
}

PathSample sampleAt(const Track& track, float distance) {
    const float wrapped = std::fmod(distance, track.length);
    const std::size_t count = track.samples.size();
    const float scaled = wrapped / track.length * static_cast<float>(count);
    const std::size_t index = static_cast<std::size_t>(std::floor(scaled)) % count;
    const std::size_t next = (index + 1) % count;
    const float t = scaled - std::floor(scaled);
    const PathSample& a = track.samples[index];
    const PathSample& b = track.samples[next];

    return {a.position + (b.position - a.position) * t,
            normalize(a.tangent + (b.tangent - a.tangent) * t),
            normalize(a.right + (b.right - a.right) * t), normalize(a.up + (b.up - a.up) * t)};
}

}  // namespace rollercoaster
