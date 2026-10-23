// Barebones first-person roller coaster.
//
// PATH GENERATION IN FOUR STEPS
// 1. Define a handful of control points around a closed loop.
// 2. Smooth them with a closed Catmull-Rom spline.
// 3. Resample the spline at equal distances so camera speed is in metres.
// 4. Build a tangent/right/up frame at each sample; bank into turns.
//
// The camera simply follows the resampled path. The track geometry uses the
// same samples, so what the camera sees and where it moves can never drift
// apart.

#include <SDL3/SDL.h>
#include <SDL3/SDL_metal.h>

#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <string>
#include <system_error>
#include <vector>

namespace {

constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;

struct Vec3 {
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};
};

Vec3 operator+(Vec3 a, Vec3 b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}
Vec3 operator-(Vec3 a, Vec3 b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}
Vec3 operator*(Vec3 a, float s) {
    return {a.x * s, a.y * s, a.z * s};
}
float dot(Vec3 a, Vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
Vec3 cross(Vec3 a, Vec3 b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
float length(Vec3 v) {
    return std::sqrt(dot(v, v));
}
Vec3 normalize(Vec3 v) {
    const float n = length(v);
    return n > 0.0001f ? v * (1.0f / n) : Vec3{0.0f, 1.0f, 0.0f};
}

struct Mat4 {
    float m[16]{};
};

Mat4 multiply(const Mat4& a, const Mat4& b) {
    Mat4 result{};
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k) {
                sum += a.m[k * 4 + row] * b.m[column * 4 + k];
            }
            result.m[column * 4 + row] = sum;
        }
    }
    return result;
}

Mat4 perspective(float fovY, float aspect, float nearPlane, float farPlane) {
    const float f = 1.0f / std::tan(fovY * 0.5f);
    Mat4 result{};
    result.m[0] = f / aspect;
    result.m[5] = f;
    result.m[10] = farPlane / (nearPlane - farPlane);
    result.m[11] = -1.0f;
    result.m[14] = (farPlane * nearPlane) / (nearPlane - farPlane);
    return result;
}

Mat4 lookAt(Vec3 eye, Vec3 centre, Vec3 up) {
    const Vec3 forward = normalize(centre - eye);
    const Vec3 right = normalize(cross(forward, up));
    const Vec3 correctedUp = cross(right, forward);
    Mat4 result{};
    result.m[0] = right.x;
    result.m[4] = right.y;
    result.m[8] = right.z;
    result.m[12] = -dot(right, eye);
    result.m[1] = correctedUp.x;
    result.m[5] = correctedUp.y;
    result.m[9] = correctedUp.z;
    result.m[13] = -dot(correctedUp, eye);
    result.m[2] = -forward.x;
    result.m[6] = -forward.y;
    result.m[10] = -forward.z;
    result.m[14] = dot(forward, eye);
    result.m[15] = 1.0f;
    return result;
}

struct PathSample {
    Vec3 position;
    Vec3 tangent;
    Vec3 right;
    Vec3 up;
};

struct Track {
    std::vector<PathSample> samples;
    float length{0.0f};
};

// Closed Catmull-Rom. The four points around a segment produce a smooth curve
// that passes through the middle two control points.
Vec3 catmullRom(Vec3 p0, Vec3 p1, Vec3 p2, Vec3 p3, float t) {
    const float t2 = t * t;
    const float t3 = t2 * t;
    return (p1 * 2.0f + (p2 - p0) * t + (p0 * 2.0f - p1 * 5.0f + p2 * 4.0f - p3) * t2 +
            (p1 * 3.0f - p0 - p2 * 3.0f + p3) * t3) *
           0.5f;
}

Track buildTrack() {
    // The only hand-authored data: a closed loop of hills and turns.
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
    constexpr int stepsPerSegment = 32;
    std::vector<Vec3> dense;
    for (int segment = 0; segment < count; ++segment) {
        const Vec3 p0 = controlPoints[(segment + count - 1) % count];
        const Vec3 p1 = controlPoints[segment];
        const Vec3 p2 = controlPoints[(segment + 1) % count];
        const Vec3 p3 = controlPoints[(segment + 2) % count];
        for (int step = 0; step < stepsPerSegment; ++step) {
            const float t = static_cast<float>(step) / static_cast<float>(stepsPerSegment);
            dense.push_back(catmullRom(p0, p1, p2, p3, t));
        }
    }

    std::vector<float> cumulative(dense.size() + 1, 0.0f);
    for (std::size_t i = 1; i <= dense.size(); ++i) {
        cumulative[i] = cumulative[i - 1] + length(dense[i % dense.size()] - dense[i - 1]);
    }
    const float total = cumulative.back();

    constexpr int sampleCount = 700;
    std::vector<Vec3> points;
    points.reserve(sampleCount);
    std::size_t segment = 0;
    for (int i = 0; i < sampleCount; ++i) {
        const float target = total * static_cast<float>(i) / static_cast<float>(sampleCount);
        while (segment + 1 < dense.size() && cumulative[segment + 1] < target) {
            ++segment;
        }
        const float segmentLength = cumulative[segment + 1] - cumulative[segment];
        const float t =
            segmentLength > 0.0001f ? (target - cumulative[segment]) / segmentLength : 0.0f;
        points.push_back(dense[segment] +
                         (dense[(segment + 1) % dense.size()] - dense[segment]) * t);
    }

    Track track;
    track.length = total;
    track.samples.reserve(sampleCount);
    for (int i = 0; i < sampleCount; ++i) {
        const Vec3 tangent =
            normalize(points[(i + 1) % sampleCount] - points[(i + sampleCount - 1) % sampleCount]);
        Vec3 right = normalize(cross(tangent, Vec3{0.0f, 1.0f, 0.0f}));
        Vec3 up = normalize(cross(right, tangent));

        // Bank into the turn. The previous tangent tells us which way the path
        // is bending; rotating the frame around the tangent leans the camera.
        const Vec3 previousTangent =
            normalize(points[i] - points[(i + sampleCount - 1) % sampleCount]);
        const float turn = dot(cross(previousTangent, tangent), Vec3{0.0f, 1.0f, 0.0f});
        const float bank = std::clamp(turn * 85.0f, -0.32f, 0.32f);
        const float cosine = std::cos(bank);
        const float sine = std::sin(bank);
        right = normalize(right * cosine + cross(tangent, right) * sine);
        up = normalize(cross(right, tangent));
        track.samples.push_back({points[i], tangent, right, up});
    }
    return track;
}

struct Vertex {
    float position[3];
    float color[3];
};

struct Mesh {
    std::vector<Vertex> vertices;
};

void addQuad(Mesh& mesh, Vec3 a, Vec3 b, Vec3 c, Vec3 d, Vec3 color) {
    const Vertex va{{a.x, a.y, a.z}, {color.x, color.y, color.z}};
    const Vertex vb{{b.x, b.y, b.z}, {color.x, color.y, color.z}};
    const Vertex vc{{c.x, c.y, c.z}, {color.x, color.y, color.z}};
    const Vertex vd{{d.x, d.y, d.z}, {color.x, color.y, color.z}};
    mesh.vertices.insert(mesh.vertices.end(), {va, vb, vc, va, vc, vd});
}

Mesh buildTrackMesh(const Track& track) {
    Mesh mesh;
    const Vec3 railColor{0.95f, 0.78f, 0.42f};
    const Vec3 tieColor{0.26f, 0.17f, 0.10f};
    const Vec3 supportColor{0.18f, 0.19f, 0.18f};

    // A big ground plane gives the eye a horizon and makes height readable.
    addQuad(mesh, {-300.0f, 0.0f, -300.0f}, {300.0f, 0.0f, -300.0f}, {300.0f, 0.0f, 300.0f},
            {-300.0f, 0.0f, 300.0f}, {0.10f, 0.16f, 0.10f});

    const std::size_t count = track.samples.size();
    for (std::size_t i = 0; i < count; ++i) {
        const PathSample& a = track.samples[i];
        const PathSample& b = track.samples[(i + 1) % count];

        // Two rails, each a thin ribbon following the centreline.
        for (int side = -1; side <= 1; side += 2) {
            const Vec3 centreA = a.position + a.right * (0.58f * static_cast<float>(side));
            const Vec3 centreB = b.position + b.right * (0.58f * static_cast<float>(side));
            const Vec3 widthA = a.right * 0.045f;
            const Vec3 widthB = b.right * 0.045f;
            addQuad(mesh, centreA - widthA, centreA + widthA, centreB + widthB, centreB - widthB,
                    railColor);
        }

        // Cross ties every few metres make the motion easy to read.
        if (i % 3 == 0) {
            const Vec3 tangentA = a.tangent * 0.07f;
            const Vec3 left = a.right * 0.84f;
            addQuad(mesh, a.position - left - tangentA, a.position + left - tangentA,
                    a.position + left + tangentA, a.position - left + tangentA, tieColor);
        }

        // Sparse supports down to the ground.
        if (i % 25 == 0) {
            const Vec3 left = a.right * 0.08f;
            const Vec3 topLeft = a.position - left;
            const Vec3 topRight = a.position + left;
            const Vec3 bottomLeft{topLeft.x, 0.0f, topLeft.z};
            const Vec3 bottomRight{topRight.x, 0.0f, topRight.z};
            addQuad(mesh, topLeft, topRight, bottomRight, bottomLeft, supportColor);
        }
    }
    return mesh;
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
    return {a.position + (b.position - a.position) * t, a.tangent + (b.tangent - a.tangent) * t,
            a.right + (b.right - a.right) * t, a.up + (b.up - a.up) * t};
}

struct alignas(16) CameraUniforms {
    float viewProjection[16];
    float cameraPosition[4];
};

static_assert(sizeof(CameraUniforms) == 80, "CameraUniforms must match the shader");

std::string readTextFile(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

NS::String* nsString(const char* text) {
    return NS::String::string(text, NS::UTF8StringEncoding);
}

std::string describeError(NS::Error* error) {
    if (error == nullptr || error->localizedDescription() == nullptr) {
        return "unknown Metal error";
    }
    return error->localizedDescription()->utf8String();
}

std::filesystem::path shaderDirectory() {
    if (const char* overridePath = std::getenv("SHADERS_METAL_DIR")) {
        return overridePath;
    }
    if (const char* basePath = SDL_GetBasePath()) {
        std::error_code error;
        const std::filesystem::path candidate =
            std::filesystem::path(basePath) / ".." / "Resources" / "shaders";
        if (std::filesystem::is_directory(candidate, error)) {
            return candidate.lexically_normal();
        }
    }
    return "Shaders";
}

void logError(const char* context) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s: %s", context, SDL_GetError());
}

}  // namespace

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    SDL_SetAppMetadata("Roller Coaster", "0.1.0", "com.dlcmh.cppgames.rollercoaster");
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "metal");

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        logError("SDL_Init failed");
        return 1;
    }

    SDL_Window* window =
        SDL_CreateWindow("Roller Coaster - first person", kWindowWidth, kWindowHeight,
                         SDL_WINDOW_METAL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (window == nullptr) {
        logError("SDL_CreateWindow failed");
        SDL_Quit();
        return 1;
    }

    SDL_MetalView view = SDL_Metal_CreateView(window);
    if (view == nullptr) {
        logError("SDL_Metal_CreateView failed");
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    CA::MetalLayer* layer = static_cast<CA::MetalLayer*>(SDL_Metal_GetLayer(view));
    MTL::Device* device = MTL::CreateSystemDefaultDevice();
    MTL::CommandQueue* queue = device != nullptr ? device->newCommandQueue() : nullptr;
    if (layer == nullptr || device == nullptr || queue == nullptr) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not create Metal device or queue.");
        SDL_Metal_DestroyView(view);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    layer->setDevice(device);
    layer->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
    layer->setFramebufferOnly(true);
    layer->setDisplaySyncEnabled(true);

    const Track track = buildTrack();
    const Mesh mesh = buildTrackMesh(track);
    MTL::Buffer* vertexBuffer =
        device->newBuffer(mesh.vertices.data(), mesh.vertices.size() * sizeof(Vertex),
                          MTL::ResourceStorageModeShared);

    const std::string shaderSource = readTextFile(shaderDirectory() / "roller-coaster.metal");
    NS::Error* metalError = nullptr;
    MTL::Library* library =
        device->newLibrary(nsString(shaderSource.c_str()), nullptr, &metalError);
    if (library == nullptr) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Shader compilation failed: %s",
                     describeError(metalError).c_str());
        vertexBuffer->release();
        queue->release();
        device->release();
        SDL_Metal_DestroyView(view);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    MTL::Function* vertexFunction = library->newFunction(nsString("vertexMain"));
    MTL::Function* fragmentFunction = library->newFunction(nsString("fragmentMain"));
    MTL::RenderPipelineDescriptor* pipelineDescriptor =
        MTL::RenderPipelineDescriptor::alloc()->init();
    pipelineDescriptor->setVertexFunction(vertexFunction);
    pipelineDescriptor->setFragmentFunction(fragmentFunction);
    pipelineDescriptor->setDepthAttachmentPixelFormat(MTL::PixelFormatDepth32Float);
    pipelineDescriptor->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);

    MTL::VertexDescriptor* vertexDescriptor = MTL::VertexDescriptor::alloc()->init();
    vertexDescriptor->attributes()->object(0)->setFormat(MTL::VertexFormatFloat3);
    vertexDescriptor->attributes()->object(0)->setOffset(0);
    vertexDescriptor->attributes()->object(0)->setBufferIndex(2);
    vertexDescriptor->attributes()->object(1)->setFormat(MTL::VertexFormatFloat3);
    vertexDescriptor->attributes()->object(1)->setOffset(sizeof(float) * 3);
    vertexDescriptor->attributes()->object(1)->setBufferIndex(2);
    vertexDescriptor->layouts()->object(2)->setStride(sizeof(Vertex));
    pipelineDescriptor->setVertexDescriptor(vertexDescriptor);

    MTL::RenderPipelineState* pipeline =
        device->newRenderPipelineState(pipelineDescriptor, &metalError);
    if (pipeline == nullptr) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Pipeline creation failed: %s",
                     describeError(metalError).c_str());
        vertexDescriptor->release();
        pipelineDescriptor->release();
        vertexFunction->release();
        fragmentFunction->release();
        library->release();
        vertexBuffer->release();
        queue->release();
        device->release();
        SDL_Metal_DestroyView(view);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    MTL::DepthStencilDescriptor* depthDescriptor = MTL::DepthStencilDescriptor::alloc()->init();
    depthDescriptor->setDepthCompareFunction(MTL::CompareFunctionLess);
    depthDescriptor->setDepthWriteEnabled(true);
    MTL::DepthStencilState* depthState = device->newDepthStencilState(depthDescriptor);
    depthDescriptor->release();
    vertexDescriptor->release();
    pipelineDescriptor->release();
    vertexFunction->release();
    fragmentFunction->release();
    library->release();

    int pixelWidth = 0;
    int pixelHeight = 0;
    MTL::Texture* depthTexture = nullptr;
    auto resize = [&](int width, int height) {
        if (width <= 0 || height <= 0 || (width == pixelWidth && height == pixelHeight)) {
            return;
        }
        pixelWidth = width;
        pixelHeight = height;
        layer->setDrawableSize(
            CGSizeMake(static_cast<CGFloat>(width), static_cast<CGFloat>(height)));
        if (depthTexture != nullptr) {
            depthTexture->release();
        }
        MTL::TextureDescriptor* descriptor = MTL::TextureDescriptor::alloc()->init();
        descriptor->setTextureType(MTL::TextureType2D);
        descriptor->setPixelFormat(MTL::PixelFormatDepth32Float);
        descriptor->setWidth(static_cast<NS::UInteger>(width));
        descriptor->setHeight(static_cast<NS::UInteger>(height));
        descriptor->setUsage(MTL::TextureUsageRenderTarget);
        descriptor->setStorageMode(MTL::StorageModePrivate);
        depthTexture = device->newTexture(descriptor);
        descriptor->release();
    };
    resize(pixelWidth, pixelHeight);

    SDL_ShowWindow(window);
    SDL_RaiseWindow(window);

    float distance = 0.0f;
    bool paused = false;
    bool running = true;
    Uint64 previousCounter = SDL_GetPerformanceCounter();
    const Uint64 frequency = SDL_GetPerformanceFrequency();
    const float startHeight = track.samples.front().position.y;

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            } else if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
                if (event.key.key == SDLK_ESCAPE || event.key.key == SDLK_Q) {
                    running = false;
                } else if (event.key.key == SDLK_P) {
                    paused = !paused;
                } else if (event.key.key == SDLK_R) {
                    distance = 0.0f;
                } else if (event.key.key == SDLK_F || event.key.key == SDLK_F11) {
                    const bool fullscreen =
                        (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0;
                    SDL_SetWindowFullscreen(window, !fullscreen);
                }
            }
        }

        const Uint64 now = SDL_GetPerformanceCounter();
        const float delta =
            static_cast<float>(now - previousCounter) / static_cast<float>(frequency);
        previousCounter = now;

        int width = 0;
        int height = 0;
        SDL_GetWindowSizeInPixels(window, &width, &height);
        resize(width, height);
        if (depthTexture == nullptr) {
            continue;
        }

        if (!paused) {
            const PathSample current = sampleAt(track, distance);
            // Simple energy model: faster low down, slower over the crest.
            const float speed = std::clamp(
                std::sqrt(2.0f * 9.81f * std::max(0.5f, startHeight - current.position.y)), 5.0f,
                15.0f);
            distance = std::fmod(distance + speed * delta, track.length);
        }

        const PathSample current = sampleAt(track, distance);
        const Vec3 eye = current.position + current.up * 1.1f;
        const Vec3 target = current.position + current.tangent * 8.0f + current.up * 1.1f;
        const Mat4 view = lookAt(eye, target, current.up);
        const float aspect = static_cast<float>(pixelWidth) / static_cast<float>(pixelHeight);
        const Mat4 projection = perspective(1.05f, aspect, 0.1f, 600.0f);

        CameraUniforms uniforms{};
        const Mat4 viewProjection = multiply(projection, view);
        std::copy(std::begin(viewProjection.m), std::end(viewProjection.m),
                  std::begin(uniforms.viewProjection));
        uniforms.cameraPosition[0] = eye.x;
        uniforms.cameraPosition[1] = eye.y;
        uniforms.cameraPosition[2] = eye.z;

        CA::MetalDrawable* drawable = layer->nextDrawable();
        if (drawable == nullptr) {
            continue;
        }

        MTL::RenderPassDescriptor* pass = MTL::RenderPassDescriptor::alloc()->init();
        MTL::RenderPassColorAttachmentDescriptor* color = pass->colorAttachments()->object(0);
        color->setTexture(drawable->texture());
        color->setLoadAction(MTL::LoadActionClear);
        color->setStoreAction(MTL::StoreActionStore);
        color->setClearColor(MTL::ClearColor(0.56, 0.68, 0.82, 1.0));
        pass->depthAttachment()->setTexture(depthTexture);
        pass->depthAttachment()->setLoadAction(MTL::LoadActionClear);
        pass->depthAttachment()->setStoreAction(MTL::StoreActionStore);
        pass->depthAttachment()->setClearDepth(1.0);

        MTL::CommandBuffer* commandBuffer = queue->commandBuffer();
        MTL::RenderCommandEncoder* encoder = commandBuffer->renderCommandEncoder(pass);
        encoder->setRenderPipelineState(pipeline);
        encoder->setDepthStencilState(depthState);
        encoder->setCullMode(MTL::CullModeNone);
        encoder->setVertexBuffer(vertexBuffer, 0, 2);
        encoder->setVertexBytes(&uniforms, sizeof(uniforms), 0);
        encoder->setFragmentBytes(&uniforms, sizeof(uniforms), 0);
        encoder->drawPrimitives(MTL::PrimitiveTypeTriangle, static_cast<NS::UInteger>(0),
                                static_cast<NS::UInteger>(mesh.vertices.size()));
        encoder->endEncoding();
        commandBuffer->presentDrawable(drawable);
        commandBuffer->commit();
        pass->release();

        if (paused) {
            SDL_SetWindowTitle(window, "Roller Coaster - first person  [paused]");
        } else {
            SDL_SetWindowTitle(window, "Roller Coaster - first person");
        }
    }

    if (depthTexture != nullptr) {
        depthTexture->release();
    }
    pipeline->release();
    depthState->release();
    vertexBuffer->release();
    queue->release();
    device->release();
    SDL_Metal_DestroyView(view);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
