#include "AnimationHandler.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <limits>
#include <string>

#include <glm/common.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>

#include "JSON.hpp"

namespace {
    const JSON::Value* FindObjValue(const JSON::Value& v, const std::string& key) {
        if(!v.IsObject()) {
            return nullptr;
        }
        return v.AsObject().Find(key);
    }

    bool AsFloat(const JSON::Value& v, float& out) {
        if(!v.IsNumber()) {
            return false;
        }
        out = static_cast<float>(v.AsNumber());
        return true;
    }

    bool AsBool(const JSON::Value& v, bool& out) {
        if(v.IsBool()) {
            out = v.AsBool();
            return true;
        }
        if(v.IsString()) {
            const std::string& s = v.AsString();
            if(s == "loop" || s == "true") {
                out = true;
                return true;
            }
            if(s == "once" || s == "false") {
                out = false;
                return true;
            }
        }
        return false;
    }

    bool ParseVec3(const JSON::Value& v, glm::vec3& out) {
        if(v.IsArray()) {
            const auto& arr = v.AsArray().values;
            if(arr.size() < 3 || !arr[0].IsNumber() || !arr[1].IsNumber() || !arr[2].IsNumber()) {
                return false;
            }
            out = glm::vec3(
                static_cast<float>(arr[0].AsNumber()),
                static_cast<float>(arr[1].AsNumber()),
                static_cast<float>(arr[2].AsNumber())
            );
            return true;
        }

        if(v.IsObject()) {
            const JSON::Value* vectorValue = v.AsObject().Find("vector");
            if(!vectorValue) {
                vectorValue = v.AsObject().Find("post");
            }
            if(vectorValue && ParseVec3(*vectorValue, out)) {
                return true;
            }

            const JSON::Value* x = v.AsObject().Find("x");
            const JSON::Value* y = v.AsObject().Find("y");
            const JSON::Value* z = v.AsObject().Find("z");
            if(x && y && z && x->IsNumber() && y->IsNumber() && z->IsNumber()) {
                out = glm::vec3(
                    static_cast<float>(x->AsNumber()),
                    static_cast<float>(y->AsNumber()),
                    static_cast<float>(z->AsNumber())
                );
                return true;
            }
        }

        return false;
    }

    bool ParseChannelKeyframeEntry(
        const JSON::Value& entryValue,
        float explicitTime,
        bool explicitHasTime,
        std::vector<AnimationHandler::KeyframeVec3>& outKeyframes
    ) {
        AnimationHandler::KeyframeVec3 keyframe;
        keyframe.timeSeconds = explicitHasTime ? explicitTime : 0.0f;

        if(ParseVec3(entryValue, keyframe.value)) {
            outKeyframes.push_back(keyframe);
            return true;
        }

        if(!entryValue.IsObject()) {
            return false;
        }

        bool foundVector = false;
        if(const JSON::Value* vectorValue = entryValue.AsObject().Find("vector")) {
            foundVector = ParseVec3(*vectorValue, keyframe.value);
        }
        if(!foundVector) {
            if(const JSON::Value* postValue = entryValue.AsObject().Find("post")) {
                foundVector = ParseVec3(*postValue, keyframe.value);
            }
        }
        if(!foundVector) {
            return false;
        }

        if(const JSON::Value* timeValue = entryValue.AsObject().Find("time")) {
            float t = 0.0f;
            if(AsFloat(*timeValue, t)) {
                keyframe.timeSeconds = t;
            }
        }

        if(const JSON::Value* lerp = entryValue.AsObject().Find("lerp_mode")) {
            if(lerp->IsString() && lerp->AsString() == "step") {
                keyframe.stepInterpolation = true;
            }
        }

        outKeyframes.push_back(keyframe);
        return true;
    }

    void ParseBoneChannel(const JSON::Value& channelValue, std::vector<AnimationHandler::KeyframeVec3>& outKeyframes) {
        outKeyframes.clear();

        if(channelValue.IsArray() || (channelValue.IsObject() && channelValue.AsObject().Find("vector"))) {
            ParseChannelKeyframeEntry(channelValue, 0.0f, false, outKeyframes);
            return;
        }

        if(!channelValue.IsObject()) {
            return;
        }

        const auto& entries = channelValue.AsObject().entries;
        for(const auto& [timeKey, entryValue] : entries) {
            char* end = nullptr;
            const float parsedTime = std::strtof(timeKey.c_str(), &end);
            if(end != timeKey.c_str() && end && *end == '\0') {
                ParseChannelKeyframeEntry(entryValue, parsedTime, true, outKeyframes);
                continue;
            }

            if(timeKey == "vector" || timeKey == "post") {
                AnimationHandler::KeyframeVec3 keyframe;
                keyframe.timeSeconds = 0.0f;
                if(ParseVec3(entryValue, keyframe.value)) {
                    outKeyframes.push_back(keyframe);
                }
            }
        }
    }

    float WrapAnimationTime(float timeSeconds, bool loop, float lengthSeconds) {
        if(lengthSeconds <= 0.0f) {
            return std::max(0.0f, timeSeconds);
        }
        if(!loop) {
            return glm::clamp(timeSeconds, 0.0f, lengthSeconds);
        }
        float wrapped = std::fmod(timeSeconds, lengthSeconds);
        if(wrapped < 0.0f) {
            wrapped += lengthSeconds;
        }
        return wrapped;
    }
}

bool AnimationHandler::LoadClipFromFile(const std::string& clipName, const std::string& filePath) {
    std::string error;
    const std::optional<JSON::Value> root = JSON::ParseFile(filePath, &error);
    if(!root || !root->IsObject()) {
        return false;
    }

    const JSON::Value* clipObject = &(*root);
    std::string discoveredName = clipName;

    if(const JSON::Value* animations = FindObjValue(*root, "animations"); animations && animations->IsObject()) {
        if(!animations->AsObject().entries.empty()) {
            discoveredName = animations->AsObject().entries.front().first;
            clipObject = &animations->AsObject().entries.front().second;
        }
    }

    if(!clipObject || !clipObject->IsObject()) {
        return false;
    }

    Clip clip;
    clip.name = discoveredName;
    clip.lengthSeconds = 0.0f;
    clip.loop = true;

    if(const JSON::Value* v = clipObject->AsObject().Find("animation_length"); v && v->IsNumber()) {
        clip.lengthSeconds = static_cast<float>(v->AsNumber());
    }
    if(const JSON::Value* v = clipObject->AsObject().Find("loop")) {
        bool loop = true;
        if(AsBool(*v, loop)) {
            clip.loop = loop;
        }
    }

    if(const JSON::Value* bonesValue = clipObject->AsObject().Find("bones"); bonesValue && bonesValue->IsObject()) {
        for(const auto& [boneName, boneValue] : bonesValue->AsObject().entries) {
            if(!boneValue.IsObject()) {
                continue;
            }

            BoneChannel channel;

            if(const JSON::Value* t = boneValue.AsObject().Find("position")) {
                ParseBoneChannel(*t, channel.translation);
                SortAndNormalizeChannel(channel.translation);
            }
            if(const JSON::Value* r = boneValue.AsObject().Find("rotation")) {
                ParseBoneChannel(*r, channel.rotationDegrees);
                SortAndNormalizeChannel(channel.rotationDegrees);
            }
            if(const JSON::Value* s = boneValue.AsObject().Find("scale")) {
                ParseBoneChannel(*s, channel.scale);
                SortAndNormalizeChannel(channel.scale);
            }

            if(!channel.translation.empty() || !channel.rotationDegrees.empty() || !channel.scale.empty()) {
                clip.bones[boneName] = std::move(channel);
            }
        }
    }

    if(clip.lengthSeconds <= 0.0f) {
        float inferred = 0.0f;
        for(const auto& [_, bone] : clip.bones) {
            if(!bone.translation.empty()) {
                inferred = std::max(inferred, bone.translation.back().timeSeconds);
            }
            if(!bone.rotationDegrees.empty()) {
                inferred = std::max(inferred, bone.rotationDegrees.back().timeSeconds);
            }
            if(!bone.scale.empty()) {
                inferred = std::max(inferred, bone.scale.back().timeSeconds);
            }
        }
        clip.lengthSeconds = std::max(0.1f, inferred);
    }

    clips_[clipName] = std::move(clip);
    return true;
}

bool AnimationHandler::LoadClipsFromDirectory(const std::string& directoryPath) {
    namespace fs = std::filesystem;
    const fs::path dirPath(directoryPath);
    if(!fs::exists(dirPath) || !fs::is_directory(dirPath)) {
        return false;
    }

    bool loadedAtLeastOne = false;
    for(const auto& entry : fs::directory_iterator(dirPath)) {
        if(!entry.is_regular_file()) {
            continue;
        }
        if(entry.path().extension() != ".json") {
            continue;
        }
        const std::string clipName = entry.path().stem().string();
        if(LoadClipFromFile(clipName, entry.path().string())) {
            loadedAtLeastOne = true;
        }
    }
    return loadedAtLeastOne;
}

bool AnimationHandler::AddOrReplaceClip(Clip clip) {
    for(auto& [_, channel] : clip.bones) {
        SortAndNormalizeChannel(channel.translation);
        SortAndNormalizeChannel(channel.rotationDegrees);
        SortAndNormalizeChannel(channel.scale);
    }

    if(clip.lengthSeconds <= 0.0f) {
        float inferred = 0.0f;
        for(const auto& [_, bone] : clip.bones) {
            if(!bone.translation.empty()) {
                inferred = std::max(inferred, bone.translation.back().timeSeconds);
            }
            if(!bone.rotationDegrees.empty()) {
                inferred = std::max(inferred, bone.rotationDegrees.back().timeSeconds);
            }
            if(!bone.scale.empty()) {
                inferred = std::max(inferred, bone.scale.back().timeSeconds);
            }
        }
        clip.lengthSeconds = std::max(0.1f, inferred);
    }

    if(clip.name.empty()) {
        return false;
    }
    clips_[clip.name] = std::move(clip);
    return true;
}

void AnimationHandler::Clear() {
    clips_.clear();
}

const AnimationHandler::Clip* AnimationHandler::FindClip(const std::string& clipName) const {
    const auto it = clips_.find(clipName);
    return (it == clips_.end()) ? nullptr : &it->second;
}

std::vector<std::string> AnimationHandler::ClipNames() const {
    std::vector<std::string> names;
    names.reserve(clips_.size());
    for(const auto& [name, _] : clips_) {
        names.push_back(name);
    }
    std::sort(names.begin(), names.end());
    return names;
}

void AnimationHandler::SortAndNormalizeChannel(std::vector<KeyframeVec3>& keyframes) {
    std::sort(keyframes.begin(), keyframes.end(), [](const KeyframeVec3& a, const KeyframeVec3& b) {
        return a.timeSeconds < b.timeSeconds;
    });

    for(KeyframeVec3& keyframe : keyframes) {
        if(!std::isfinite(keyframe.timeSeconds)) {
            keyframe.timeSeconds = 0.0f;
        }
        if(keyframe.timeSeconds < 0.0f) {
            keyframe.timeSeconds = 0.0f;
        }
    }
}

bool AnimationHandler::SampleVec3Channel(
    const std::vector<KeyframeVec3>& channel,
    float timeSeconds,
    bool loop,
    float lengthSeconds,
    glm::vec3& outValue
) {
    if(channel.empty()) {
        return false;
    }

    if(channel.size() == 1) {
        outValue = channel.front().value;
        return true;
    }

    const float t = WrapAnimationTime(timeSeconds, loop, lengthSeconds);
    if(t <= channel.front().timeSeconds) {
        outValue = channel.front().value;
        return true;
    }
    if(t >= channel.back().timeSeconds) {
        outValue = channel.back().value;
        return true;
    }

    auto upperIt = std::upper_bound(channel.begin(), channel.end(), t, [](float value, const KeyframeVec3& keyframe) {
        return value < keyframe.timeSeconds;
    });

    if(upperIt == channel.begin() || upperIt == channel.end()) {
        outValue = (upperIt == channel.end()) ? channel.back().value : upperIt->value;
        return true;
    }

    const KeyframeVec3& b = *upperIt;
    const KeyframeVec3& a = *(upperIt - 1);

    const float span = std::max(0.0001f, b.timeSeconds - a.timeSeconds);
    const float alpha = glm::clamp((t - a.timeSeconds) / span, 0.0f, 1.0f);

    if(a.stepInterpolation) {
        outValue = a.value;
    } else {
        outValue = glm::mix(a.value, b.value, alpha);
    }
    return true;
}

bool AnimationHandler::SampleBone(const Clip& clip, const std::string& boneName, float timeSeconds, SampledBonePose& outPose) const {
    outPose = SampledBonePose{};

    const auto boneIt = clip.bones.find(boneName);
    if(boneIt == clip.bones.end()) {
        return false;
    }

    const BoneChannel& channel = boneIt->second;

    glm::vec3 v;
    if(SampleVec3Channel(channel.translation, timeSeconds, clip.loop, clip.lengthSeconds, v)) {
        outPose.translation = v;
        outPose.hasTranslation = true;
    }

    if(SampleVec3Channel(channel.rotationDegrees, timeSeconds, clip.loop, clip.lengthSeconds, v)) {
        const glm::vec3 radians = glm::radians(v);
        outPose.rotation = glm::quat(radians);
        outPose.hasRotation = true;
    }

    if(SampleVec3Channel(channel.scale, timeSeconds, clip.loop, clip.lengthSeconds, v)) {
        outPose.scale = v;
        outPose.hasScale = true;
    }

    return outPose.hasTranslation || outPose.hasRotation || outPose.hasScale;
}
