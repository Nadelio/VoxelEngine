#include "Shader.hpp"

#include <fstream>
#include <sstream>
#include <vector>

namespace {
    PFNGLCREATESHADERPROC pglCreateShader = nullptr;
    PFNGLSHADERSOURCEPROC pglShaderSource = nullptr;
    PFNGLCOMPILESHADERPROC pglCompileShader = nullptr;
    PFNGLGETSHADERIVPROC pglGetShaderiv = nullptr;
    PFNGLGETSHADERINFOLOGPROC pglGetShaderInfoLog = nullptr;
    PFNGLCREATEPROGRAMPROC pglCreateProgram = nullptr;
    PFNGLATTACHSHADERPROC pglAttachShader = nullptr;
    PFNGLLINKPROGRAMPROC pglLinkProgram = nullptr;
    PFNGLGETPROGRAMIVPROC pglGetProgramiv = nullptr;
    PFNGLGETPROGRAMINFOLOGPROC pglGetProgramInfoLog = nullptr;
    PFNGLUSEPROGRAMPROC pglUseProgram = nullptr;
    PFNGLDELETESHADERPROC pglDeleteShader = nullptr;
    PFNGLDELETEPROGRAMPROC pglDeleteProgram = nullptr;
    PFNGLGETUNIFORMLOCATIONPROC pglGetUniformLocation = nullptr;
    PFNGLUNIFORMMATRIX4FVPROC pglUniformMatrix4fv = nullptr;
    PFNGLUNIFORM1IPROC pglUniform1i = nullptr;
    PFNGLUNIFORM4FPROC pglUniform4f = nullptr;
    PFNGLUNIFORM2FPROC pglUniform2f = nullptr;

    bool gLoaded = false;

    SDL_FunctionPointer GLProc(const char* name) {
        return SDL_GL_GetProcAddress(name);
    }
}

Shader::~Shader() {
    if (program_ != 0 && pglDeleteProgram) {
        pglDeleteProgram(program_);
    }
}

bool Shader::LoadOpenGLFunctions() {
    if (gLoaded) {
        return true;
    }

    pglCreateShader = reinterpret_cast<PFNGLCREATESHADERPROC>(GLProc("glCreateShader"));
    pglShaderSource = reinterpret_cast<PFNGLSHADERSOURCEPROC>(GLProc("glShaderSource"));
    pglCompileShader = reinterpret_cast<PFNGLCOMPILESHADERPROC>(GLProc("glCompileShader"));
    pglGetShaderiv = reinterpret_cast<PFNGLGETSHADERIVPROC>(GLProc("glGetShaderiv"));
    pglGetShaderInfoLog = reinterpret_cast<PFNGLGETSHADERINFOLOGPROC>(GLProc("glGetShaderInfoLog"));
    pglCreateProgram = reinterpret_cast<PFNGLCREATEPROGRAMPROC>(GLProc("glCreateProgram"));
    pglAttachShader = reinterpret_cast<PFNGLATTACHSHADERPROC>(GLProc("glAttachShader"));
    pglLinkProgram = reinterpret_cast<PFNGLLINKPROGRAMPROC>(GLProc("glLinkProgram"));
    pglGetProgramiv = reinterpret_cast<PFNGLGETPROGRAMIVPROC>(GLProc("glGetProgramiv"));
    pglGetProgramInfoLog = reinterpret_cast<PFNGLGETPROGRAMINFOLOGPROC>(GLProc("glGetProgramInfoLog"));
    pglUseProgram = reinterpret_cast<PFNGLUSEPROGRAMPROC>(GLProc("glUseProgram"));
    pglDeleteShader = reinterpret_cast<PFNGLDELETESHADERPROC>(GLProc("glDeleteShader"));
    pglDeleteProgram = reinterpret_cast<PFNGLDELETEPROGRAMPROC>(GLProc("glDeleteProgram"));
    pglGetUniformLocation = reinterpret_cast<PFNGLGETUNIFORMLOCATIONPROC>(GLProc("glGetUniformLocation"));
    pglUniformMatrix4fv = reinterpret_cast<PFNGLUNIFORMMATRIX4FVPROC>(GLProc("glUniformMatrix4fv"));
    pglUniform1i = reinterpret_cast<PFNGLUNIFORM1IPROC>(GLProc("glUniform1i"));
    pglUniform4f = reinterpret_cast<PFNGLUNIFORM4FPROC>(GLProc("glUniform4f"));
    pglUniform2f = reinterpret_cast<PFNGLUNIFORM2FPROC>(GLProc("glUniform2f"));

    gLoaded = pglCreateShader && pglShaderSource && pglCompileShader && pglGetShaderiv &&
              pglGetShaderInfoLog && pglCreateProgram && pglAttachShader && pglLinkProgram &&
              pglGetProgramiv && pglGetProgramInfoLog && pglUseProgram && pglDeleteShader &&
              pglDeleteProgram && pglGetUniformLocation && pglUniformMatrix4fv && pglUniform1i &&
              pglUniform4f && pglUniform2f;

    return gLoaded;
}

std::string Shader::ReadTextFile(const std::string& path) {
    std::ifstream input(path);
    if (!input) {
        SDL_Log("Could not open shader file: %s", path.c_str());
        return {};
    }

    std::stringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

bool Shader::CheckShaderCompile(GLuint shader, const char* label) {
    GLint ok = 0;
    pglGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (ok == GL_TRUE) {
        return true;
    }

    GLint logLength = 0;
    pglGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
    std::vector<char> log(static_cast<size_t>(logLength > 1 ? logLength : 1));
    pglGetShaderInfoLog(shader, logLength, nullptr, log.data());
    SDL_Log("Shader compile failed (%s): %s", label, log.data());
    return false;
}

bool Shader::CheckProgramLink(GLuint program) {
    GLint ok = 0;
    pglGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (ok == GL_TRUE) {
        return true;
    }

    GLint logLength = 0;
    pglGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
    std::vector<char> log(static_cast<size_t>(logLength > 1 ? logLength : 1));
    pglGetProgramInfoLog(program, logLength, nullptr, log.data());
    SDL_Log("Program link failed: %s", log.data());
    return false;
}

bool Shader::LoadFromFiles(const std::string& vertexPath, const std::string& fragmentPath) {
    if (!LoadOpenGLFunctions()) {
        SDL_Log("Failed to load OpenGL shader functions.");
        return false;
    }

    const std::string vertexSrc = ReadTextFile(vertexPath);
    const std::string fragmentSrc = ReadTextFile(fragmentPath);
    if (vertexSrc.empty() || fragmentSrc.empty()) {
        return false;
    }

    const char* vPtr = vertexSrc.c_str();
    const char* fPtr = fragmentSrc.c_str();

    GLuint vertexShader = pglCreateShader(GL_VERTEX_SHADER);
    GLuint fragmentShader = pglCreateShader(GL_FRAGMENT_SHADER);

    pglShaderSource(vertexShader, 1, &vPtr, nullptr);
    pglCompileShader(vertexShader);
    if (!CheckShaderCompile(vertexShader, "vertex")) {
        pglDeleteShader(vertexShader);
        pglDeleteShader(fragmentShader);
        return false;
    }

    pglShaderSource(fragmentShader, 1, &fPtr, nullptr);
    pglCompileShader(fragmentShader);
    if (!CheckShaderCompile(fragmentShader, "fragment")) {
        pglDeleteShader(vertexShader);
        pglDeleteShader(fragmentShader);
        return false;
    }

    program_ = pglCreateProgram();
    pglAttachShader(program_, vertexShader);
    pglAttachShader(program_, fragmentShader);
    pglLinkProgram(program_);

    pglDeleteShader(vertexShader);
    pglDeleteShader(fragmentShader);

    if (!CheckProgramLink(program_)) {
        pglDeleteProgram(program_);
        program_ = 0;
        return false;
    }

    return true;
}

void Shader::Use() const {
    if (program_ != 0) {
        pglUseProgram(program_);
    }
}

void Shader::SetMat4(const char* name, const float* matrix) const {
    if (program_ == 0) {
        return;
    }

    const GLint loc = pglGetUniformLocation(program_, name);
    if (loc >= 0) {
        pglUniformMatrix4fv(loc, 1, GL_FALSE, matrix);
    }
}

void Shader::SetInt(const char* name, int value) const {
    if (program_ == 0) {
        return;
    }

    const GLint loc = pglGetUniformLocation(program_, name);
    if (loc >= 0) {
        pglUniform1i(loc, value);
    }
}

void Shader::SetVec4(const char* name, float x, float y, float z, float w) const {
    if (program_ == 0) {
        return;
    }

    const GLint loc = pglGetUniformLocation(program_, name);
    if (loc >= 0) {
        pglUniform4f(loc, x, y, z, w);
    }
}

void Shader::SetVec2(const char* name, float x, float y) const {
    if (!pglGetUniformLocation || !pglUniform2f || program_ == 0) return;
    const GLint loc = pglGetUniformLocation(program_, name);
    if (loc >= 0) pglUniform2f(loc, x, y);
}
