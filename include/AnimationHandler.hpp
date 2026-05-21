#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

class AnimationHandler {
public:
    struct KeyframeVec3 {
        float timeSeconds = 0.0f;
        glm::vec3 value{0.0f};
        bool stepInterpolation = false;
    };

    struct BoneChannel {
        std::vector<KeyframeVec3> translation;
        std::vector<KeyframeVec3> rotationDegrees;
        std::vector<KeyframeVec3> scale;
    };

    struct Clip {
        std::string name;
        float lengthSeconds = 0.0f;
        bool loop = true;
        std::unordered_map<std::string, BoneChannel> bones;
    };

    struct SampledBonePose {
        glm::vec3 translation{0.0f};
        glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        glm::vec3 scale{1.0f, 1.0f, 1.0f};
        bool hasTranslation = false;
        bool hasRotation = false;
        bool hasScale = false;
    };

    bool LoadClipFromFile(const std::string& clipName, const std::string& filePath);
    bool LoadClipsFromDirectory(const std::string& directoryPath);
    bool AddOrReplaceClip(Clip clip);
    void Clear();

    const Clip* FindClip(const std::string& clipName) const;
    std::vector<std::string> ClipNames() const;

    bool SampleBone(const Clip& clip, const std::string& boneName, float timeSeconds, SampledBonePose& outPose) const;

private:
    static void SortAndNormalizeChannel(std::vector<KeyframeVec3>& keyframes);
    static bool SampleVec3Channel(const std::vector<KeyframeVec3>& channel, float timeSeconds, bool loop, float lengthSeconds, glm::vec3& outValue);

    std::unordered_map<std::string, Clip> clips_;
};
