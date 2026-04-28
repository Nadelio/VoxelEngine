#pragma once

#include <string>

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

class Shader {
public:
    Shader() = default;
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    bool LoadFromFiles(const std::string& vertexPath, const std::string& fragmentPath);
    void Use() const;
    GLuint Program() const { return program_; }

    void SetMat4(const char* name, const float* matrix) const;
    void SetInt(const char* name, int value) const;
    void SetVec4(const char* name, float x, float y, float z, float w) const;
        void SetVec2(const char* name, float x, float y) const;

private:
    static bool LoadOpenGLFunctions();
    static std::string ReadTextFile(const std::string& path);
    static bool CheckShaderCompile(GLuint shader, const char* label);
    static bool CheckProgramLink(GLuint program);

    GLuint program_ = 0;
};
