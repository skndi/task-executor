#include <cassert>
#define _CRT_SECURE_NO_WARNINGS
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include <atomic>
#include <chrono>
#include <cmath>
#include <iostream>
#include <map>
#include <random>
#include <thread>
#include <vector>

#include "Executor.hpp"
#include "Image.hpp"
#include "Material.hpp"
#include "Mesh.hpp"
#include "Primitive.hpp"
#include "Task.hpp"
#include "TaskSystem.hpp"
#include "Threading.hpp"
#include "third_party/stb_image_write.h"

/// Camera description, can be pointed at point, used to generate screen rays
struct Camera {
    const vec3 worldUp = {0, 1, 0};
    float aspect;
    vec3 origin;
    vec3 llc;
    vec3 left;
    vec3 up;

    void lookAt(float verticalFov, const vec3 &lookFrom, const vec3 &lookAt) {
        origin = lookFrom;
        const float theta = degToRad(verticalFov);
        float half_height = tan(theta / 2);
        const float half_width = aspect * half_height;

        const vec3 w = (origin - lookAt).normalized();
        const vec3 u = cross(worldUp, w).normalized();
        const vec3 v = cross(w, u);
        llc = origin - half_width * u - half_height * v - w;
        left = 2 * half_width * u;
        up = 2 * half_height * v;
    }

    Ray getRay(float u, float v) const {
        return Ray(origin, (llc + u * left + v * up - origin).normalized());
    }
};

vec3 raytrace(const Ray &r, Instancer &prims, int depth = 0) {
    Intersection data;
    if (prims.intersect(r, 0.001f, FLT_MAX, data)) {
        Ray scatter;
        Color attenuation;
        if (depth < MAX_RAY_DEPTH && data.material->shade(r, data, attenuation, scatter)) {
            const Color incoming = raytrace(scatter, prims, depth + 1);
            return attenuation * incoming;
        } else {
            return Color(0.f);
        }
    }
    const vec3 dir = r.dir;
    const float f = 0.5f * (dir.y + 1.f);
    return (1.f - f) * vec3(1.f) + f * vec3(0.5f, 0.7f, 1.f);
}

struct Scene {
    int width = 640;
    int height = 480;
    int samplesPerPixel = 2;
    std::string name;
    Instancer primitives;
    Camera camera;
    ImageData image;
    std::atomic<int> completedThreads = 0;

    std::mutex initMutex;
    std::vector<int> perThreadProgress;

    void onBeforeRender() {
        primitives.onBeforeRender();
    }

    void initImage(int w, int h, int spp) {
        image.init(w, h);
        width = w;
        height = h;
        samplesPerPixel = spp;
        camera.aspect = float(width) / height;
    }

    void addPrimitive(PrimPtr primitive) {
        primitives.addInstance(std::move(primitive));
    }

    bool renderStep(int threadIndex, int threadCount) {
        {
            std::lock_guard<std::mutex> lock(initMutex);
            if (perThreadProgress.empty()) {
                perThreadProgress.resize(threadCount, 0);
                for (int c = 0; c < int(perThreadProgress.size()); c++) {
                    perThreadProgress[c] = c;
                }
            }
        }

        int &idx = perThreadProgress[threadIndex];

        if (idx >= width * height) {
            if (completedThreads.load() == threadCount) {
                return true;
            } else {
                return false;
            }
        }

        const int r = (idx / width);
        const int c = idx % width;

        Color avg(0);
        for (int s = 0; s < samplesPerPixel; s++) {
            const float u = float(c + randFloat()) / float(width);
            const float v = float(r + randFloat()) / float(height);
            const Ray &ray = camera.getRay(u, v);
            const vec3 sample = raytrace(ray, primitives);
            avg += sample;
        }

        avg /= samplesPerPixel;
        image(c, height - r - 1) = Color(sqrtf(avg.x), sqrtf(avg.y), sqrtf(avg.z));

        idx += threadCount;

        if (idx >= width * height) {
            if (completedThreads.load() == threadCount - 1) {
                const std::string resultImage = name + ".png";
                const PNGImage &png = image.createPNGData();
                const int success = stbi_write_png(resultImage.c_str(),
                                                   width,
                                                   height,
                                                   PNGImage::componentCount(),
                                                   png.data.data(),
                                                   sizeof(PNGImage::Pixel) * width);
                assert(success == 1);
                completedThreads.fetch_add(1);
                return true;
            } else {
                completedThreads.fetch_add(1);
            }
        }
        return false;
    }
};

void sceneExample(Scene &scene) {
    scene.name = "example";
    scene.initImage(800, 600, 4);
    scene.camera.lookAt(90.f, {-0.1f, 5, -0.1f}, {0, 0, 0});

    SharedPrimPtr mesh(new TriangleMesh(MESH_FOLDER "/cube.obj", MaterialPtr(new Lambert{Color(1, 0, 0)})));
    Instancer *instancer = new Instancer;
    instancer->addInstance(mesh, vec3(2, 0, 0));
    instancer->addInstance(mesh, vec3(0, 0, 2));
    instancer->addInstance(mesh, vec3(2, 0, 2));
    scene.addPrimitive(PrimPtr(instancer));

    const float r = 0.6f;
    scene.addPrimitive(PrimPtr(new SpherePrim{vec3(2, 0, 0), r, MaterialPtr(new Lambert{Color(0.8, 0.3, 0.3)})}));
    scene.addPrimitive(PrimPtr(new SpherePrim{vec3(0, 0, 2), r, MaterialPtr(new Lambert{Color(0.8, 0.3, 0.3)})}));
    scene.addPrimitive(PrimPtr(new SpherePrim{vec3(0, 0, 0), r, MaterialPtr(new Lambert{Color(0.8, 0.3, 0.3)})}));
}

void sceneManyHeavyMeshes(Scene &scene) {
    scene.name = "instanced-dragons";
    const int count = 50;

    scene.initImage(1280, 720, 10);
    scene.camera.lookAt(90.f, {0, 3, -count}, {0, 3, count});

    SharedMaterialPtr instanceMaterials[] = {
        SharedMaterialPtr(new Lambert{Color(0.2, 0.7, 0.1)}),
        SharedMaterialPtr(new Lambert{Color(0.7, 0.2, 0.1)}),
        SharedMaterialPtr(new Lambert{Color(0.1, 0.2, 0.7)}),
        SharedMaterialPtr(new Metal{Color(0.8, 0.1, 0.1), 0.3f}),
        SharedMaterialPtr(new Metal{Color(0.1, 0.7, 0.1), 0.6f}),
        SharedMaterialPtr(new Metal{Color(0.1, 0.1, 0.7), 0.9f}),
    };
    const int materialCount = std::size(instanceMaterials);

    auto getRandomMaterial = [instanceMaterials, materialCount]() -> SharedMaterialPtr {
        const int rng = int(randFloat() * materialCount);
        return instanceMaterials[rng];
    };

    SharedPrimPtr mesh(new TriangleMesh(MESH_FOLDER "/dragon.obj", MaterialPtr(new Lambert{Color(1, 0, 0)})));
    Instancer *instancer = new Instancer;

    instancer->addInstance(mesh, vec3(0, 2.5, -count + 1), 0.08f, getRandomMaterial());

    for (int c = -count; c <= count; c++) {
        for (int r = -count; r <= count; r++) {
            instancer->addInstance(mesh, vec3(c, 0, r), 0.05f, getRandomMaterial());
            instancer->addInstance(mesh, vec3(c, 6, r), 0.05f, getRandomMaterial());
        }
    }

    scene.addPrimitive(PrimPtr(instancer));
}

void sceneManySimpleMeshes(Scene &scene) {
    scene.name = "instanced-cubes";
    const int count = 20;

    scene.initImage(800, 600, 2);
    scene.camera.lookAt(90.f, {0, 2, count}, {0, 0, 0});

    SharedPrimPtr mesh(new TriangleMesh(MESH_FOLDER "/cube.obj", MaterialPtr(new Lambert{Color(1, 0, 0)})));
    Instancer *instancer = new Instancer;

    for (int c = -count; c <= count; c++) {
        for (int r = -count; r <= count; r++) {
            instancer->addInstance(mesh, vec3(c, 0, r), 0.5f);
        }
    }

    scene.addPrimitive(PrimPtr(instancer));
}

void sceneHeavyMesh(Scene &scene) {
    scene.name = "dragon";
    scene.initImage(800, 600, 4);
    scene.camera.lookAt(90.f, {8, 10, 7}, {0, 0, 0});
    scene.addPrimitive(
        PrimPtr(new TriangleMesh(MESH_FOLDER "/dragon.obj", MaterialPtr(new Lambert{Color(0.2, 0.7, 0.1)}))));
}

typedef void (*SceneCreator)(Scene &);

struct Renderer : TaskSystem::Executor {
    Renderer(std::unique_ptr<TaskSystem::Task> taskToExecute) : Executor(std::move(taskToExecute)) {
        std::string sceneName = task->GetStringParam("sceneName").value();
        std::map<std::string, SceneCreator> sceneCreators = {
            {"Example", sceneExample},
            {"HeavyMesh", sceneHeavyMesh},
            {"ManySimpleMeshes", sceneManySimpleMeshes},
            {"ManyHeavyMeshes", sceneManyHeavyMeshes},
        };

        sceneCreators[sceneName](scene);
        scene.onBeforeRender();
        printf("Initialized scene [%s]\n", scene.name.c_str());
    }

    virtual ~Renderer() {}

    virtual ExecStatus ExecuteStep(int threadIndex, int threadCount) {
        return scene.renderStep(threadIndex, threadCount) ? ExecStatus::ES_Stop : ExecStatus::ES_Continue;
    };

    std::atomic<int> current = 0;
    int max = 0;
    int sleepMs = 0;
    Scene scene;
};

TaskSystem::Executor *ExecutorConstructorImpl(std::unique_ptr<TaskSystem::Task> taskToExecute) {
    return new Renderer(std::move(taskToExecute));
}

IMPLEMENT_ON_INIT() {
    ts.Register("raytracer", &ExecutorConstructorImpl);
}
