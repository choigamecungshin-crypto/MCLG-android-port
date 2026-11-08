#include "stdafx.h"

#include <GLES3/gl3.h>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <vector>

C4JRender RenderManager;

namespace {
using Mat4 = std::array<float, 16>;

struct DrawCall {
    int primitive = 0;
    int count = 0;
    int vertexType = 0;
    int pixelShader = 0;
    int texture = -1;
    Mat4 mvp{};
    std::array<float, 4> colour{{1.0f, 1.0f, 1.0f, 1.0f}};
    std::vector<std::uint8_t> vertices;
};

Mat4 identityMatrix() {
    Mat4 value{};
    value[0] = value[5] = value[10] = value[15] = 1.0f;
    return value;
}

Mat4 multiply(const Mat4& a, const Mat4& b) {
    Mat4 result{};
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            for (int k = 0; k < 4; ++k) {
                result[column * 4 + row] += a[k * 4 + row] * b[column * 4 + k];
            }
        }
    }
    return result;
}

std::array<Mat4, 3> g_matrices{{identityMatrix(), identityMatrix(), identityMatrix()}};
std::array<std::vector<Mat4>, 3> g_matrixStacks;
int g_matrixMode = 0;

GLuint g_program = 0;
GLuint g_vertexBuffer = 0;
GLint g_positionLocation = -1;
GLint g_uvLocation = -1;
GLint g_colourLocation = -1;
GLint g_mvpLocation = -1;
GLint g_textureLocation = -1;
GLint g_hasTextureLocation = -1;
GLint g_stateColourLocation = -1;
GLint g_alphaCutoffLocation = -1;

GLuint g_boundTexture = 0;
bool g_textureEnabled = false;
bool g_alphaTest = false;
float g_alphaCutoff = 0.1f;
float g_colour[4] = {1.0f, 1.0f, 1.0f, 1.0f};
float g_clearColour[4] = {0.025f, 0.035f, 0.055f, 1.0f};
int g_width = 1;
int g_height = 1;
int g_textureLevels = 1;
int g_nextList = 1;
int g_captureList = -1;
bool g_suspended = false;
std::unordered_map<int, std::vector<DrawCall>> g_lists;

constexpr int LEGACY_MODELVIEW = 0;
constexpr int LEGACY_PROJECTION = 1;
constexpr int LEGACY_TEXTURE_MATRIX = 2;
constexpr int LEGACY_BLEND = 2;
constexpr int LEGACY_CULL_FACE = 3;
constexpr int LEGACY_ALPHA_TEST = 4;
constexpr int LEGACY_DEPTH_TEST = 5;
constexpr int LEGACY_FOG = 6;
constexpr int LEGACY_LIGHTING = 7;
constexpr int LEGACY_LIGHT0 = 8;
constexpr int LEGACY_LIGHT1 = 9;
constexpr int LEGACY_TEXTURE_MIN_FILTER = 1;
constexpr int LEGACY_TEXTURE_MAG_FILTER = 2;
constexpr int LEGACY_TEXTURE_WRAP_S = 3;
constexpr int LEGACY_TEXTURE_WRAP_T = 4;

GLuint compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    if (shader == 0) return 0;
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled != GL_TRUE) {
        GLint length = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
        std::vector<char> message(static_cast<std::size_t>(length > 1 ? length : 1), 0);
        glGetShaderInfoLog(shader, length, nullptr, message.data());
        __android_log_print(ANDROID_LOG_ERROR, "MCLG-Android", "GLES shader compile failed: %s", message.data());
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

bool ensureRenderer() {
    if (g_program != 0 && g_vertexBuffer != 0) return true;

    static const char* vertexSource = R"GLSL(#version 300 es
precision highp float;
layout(location=0) in vec3 aPosition;
layout(location=1) in vec2 aTexCoord;
layout(location=2) in vec4 aColour;
uniform mat4 uMvp;
out vec2 vTexCoord;
out vec4 vColour;
void main() {
    gl_Position = uMvp * vec4(aPosition, 1.0);
    vTexCoord = aTexCoord;
    vColour = aColour;
}
)GLSL";
    static const char* fragmentSource = R"GLSL(#version 300 es
precision mediump float;
in vec2 vTexCoord;
in vec4 vColour;
uniform sampler2D uTexture;
uniform int uHasTexture;
uniform vec4 uStateColour;
uniform float uAlphaCutoff;
out vec4 fragColour;
void main() {
    vec4 colour = vColour * uStateColour;
    if (uHasTexture != 0) colour *= texture(uTexture, vTexCoord);
    if (colour.a < uAlphaCutoff) discard;
    fragColour = colour;
}
)GLSL";

    GLuint vertex = compileShader(GL_VERTEX_SHADER, vertexSource);
    if (vertex == 0) return false;
    GLuint fragment = compileShader(GL_FRAGMENT_SHADER, fragmentSource);
    if (fragment == 0) {
        glDeleteShader(vertex);
        return false;
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);
    glDeleteShader(vertex);
    glDeleteShader(fragment);

    GLint linked = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE) {
        GLint length = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
        std::vector<char> message(static_cast<std::size_t>(length > 1 ? length : 1), 0);
        glGetProgramInfoLog(program, length, nullptr, message.data());
        __android_log_print(ANDROID_LOG_ERROR, "MCLG-Android", "GLES program link failed: %s", message.data());
        glDeleteProgram(program);
        return false;
    }

    g_program = program;
    g_positionLocation = glGetAttribLocation(program, "aPosition");
    g_uvLocation = glGetAttribLocation(program, "aTexCoord");
    g_colourLocation = glGetAttribLocation(program, "aColour");
    g_mvpLocation = glGetUniformLocation(program, "uMvp");
    g_textureLocation = glGetUniformLocation(program, "uTexture");
    g_hasTextureLocation = glGetUniformLocation(program, "uHasTexture");
    g_stateColourLocation = glGetUniformLocation(program, "uStateColour");
    g_alphaCutoffLocation = glGetUniformLocation(program, "uAlphaCutoff");

    glGenBuffers(1, &g_vertexBuffer);
    return g_vertexBuffer != 0;
}

Mat4 currentMvp() {
    return multiply(g_matrices[LEGACY_PROJECTION], g_matrices[LEGACY_MODELVIEW]);
}

GLenum mapBlendFactor(int value) {
    switch (value) {
        case D3D11_BLEND_ZERO: return GL_ZERO;
        case D3D11_BLEND_ONE: return GL_ONE;
        case D3D11_BLEND_SRC_COLOR: return GL_SRC_COLOR;
        case D3D11_BLEND_INV_SRC_COLOR: return GL_ONE_MINUS_SRC_COLOR;
        case D3D11_BLEND_SRC_ALPHA: return GL_SRC_ALPHA;
        case D3D11_BLEND_INV_SRC_ALPHA: return GL_ONE_MINUS_SRC_ALPHA;
        case D3D11_BLEND_DEST_ALPHA: return GL_DST_ALPHA;
        case D3D11_BLEND_INV_DEST_ALPHA: return GL_ONE_MINUS_DST_ALPHA;
        case D3D11_BLEND_DEST_COLOR: return GL_DST_COLOR;
        case D3D11_BLEND_INV_DEST_COLOR: return GL_ONE_MINUS_DST_COLOR;
        default: return GL_ONE;
    }
}

GLenum mapDepthFunction(int value) {
    switch (value) {
        case D3D11_COMPARISON_NEVER: return GL_NEVER;
        case D3D11_COMPARISON_LESS: return GL_LESS;
        case D3D11_COMPARISON_EQUAL: return GL_EQUAL;
        case D3D11_COMPARISON_LESS_EQUAL: return GL_LEQUAL;
        case D3D11_COMPARISON_GREATER: return GL_GREATER;
        case D3D11_COMPARISON_NOT_EQUAL: return GL_NOTEQUAL;
        case D3D11_COMPARISON_GREATER_EQUAL: return GL_GEQUAL;
        case D3D11_COMPARISON_ALWAYS: return GL_ALWAYS;
        default: return GL_LEQUAL;
    }
}

void drawImmediate(const DrawCall& call) {
    if (!ensureRenderer() || call.count <= 0 || call.vertices.empty()) return;

    std::vector<std::uint8_t> expanded;
    const std::uint8_t* data = call.vertices.data();
    GLsizei vertexCount = call.count;
    GLenum primitive = GL_TRIANGLES;
    constexpr std::size_t stride = 32;

    switch (call.primitive) {
        case C4JRender::PRIMITIVE_TYPE_TRIANGLE_LIST: primitive = GL_TRIANGLES; break;
        case C4JRender::PRIMITIVE_TYPE_TRIANGLE_STRIP: primitive = GL_TRIANGLE_STRIP; break;
        case C4JRender::PRIMITIVE_TYPE_TRIANGLE_FAN: primitive = GL_TRIANGLE_FAN; break;
        case C4JRender::PRIMITIVE_TYPE_LINE_LIST: primitive = GL_LINES; break;
        case C4JRender::PRIMITIVE_TYPE_LINE_STRIP: primitive = GL_LINE_STRIP; break;
        case C4JRender::PRIMITIVE_TYPE_QUAD_LIST: {
            if (call.count % 4 != 0) return;
            expanded.reserve(static_cast<std::size_t>(call.count / 4 * 6) * stride);
            for (int i = 0; i < call.count; i += 4) {
                const int order[] = {i, i + 1, i + 2, i, i + 2, i + 3};
                for (int index : order) {
                    const std::uint8_t* vertex = data + static_cast<std::size_t>(index) * stride;
                    expanded.insert(expanded.end(), vertex, vertex + stride);
                }
            }
            data = expanded.data();
            vertexCount = call.count / 4 * 6;
            primitive = GL_TRIANGLES;
            break;
        }
        default: return;
    }

    glUseProgram(g_program);
    glBindBuffer(GL_ARRAY_BUFFER, g_vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(data == expanded.data() && !expanded.empty()
        ? expanded.size() : call.vertices.size()), data, GL_STREAM_DRAW);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(stride), reinterpret_cast<const void*>(0));
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(stride), reinterpret_cast<const void*>(12));
    glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, static_cast<GLsizei>(stride), reinterpret_cast<const void*>(20));

    glUniformMatrix4fv(g_mvpLocation, 1, GL_FALSE, call.mvp.data());
    glUniform4fv(g_stateColourLocation, 1, call.colour.data());
    glUniform1i(g_textureLocation, 0);
    glUniform1i(g_hasTextureLocation, call.texture >= 0 ? 1 : 0);
    glUniform1f(g_alphaCutoffLocation, g_alphaTest ? g_alphaCutoff : 0.0f);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, call.texture >= 0 ? static_cast<GLuint>(call.texture) : 0);
    glDrawArrays(primitive, 0, vertexCount);

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(2);
}

void captureOrDraw(int primitive, int count, void* data, int vertexType, int pixelShader) {
    if (count <= 0 || data == nullptr) return;

    // The legacy standard vertex contains position, UV, packed colour,
    // packed normal and a DWORD of alignment padding.
    constexpr std::size_t stride = 32;
    const std::size_t byteCount = static_cast<std::size_t>(count) * stride;

    DrawCall call;
    call.primitive = primitive;
    call.count = count;
    call.vertexType = vertexType;
    call.pixelShader = pixelShader;
    call.texture = g_textureEnabled ? static_cast<int>(g_boundTexture) : -1;
    call.mvp = currentMvp();
    for (int i = 0; i < 4; ++i) call.colour[static_cast<std::size_t>(i)] = g_colour[i];
    call.vertices.resize(byteCount);
    std::memcpy(call.vertices.data(), data, byteCount);

    if (g_captureList >= 0) {
        g_lists[g_captureList].push_back(std::move(call));
    } else {
        drawImmediate(call);
    }
}
} // namespace

void C4JRender::Tick() {}
void C4JRender::UpdateGamma(unsigned short) {}
void C4JRender::MatrixMode(int type) {
    if (type == LEGACY_MODELVIEW || type == LEGACY_PROJECTION || type == LEGACY_TEXTURE_MATRIX) g_matrixMode = type;
}
void C4JRender::MatrixSetIdentity() { g_matrices[g_matrixMode] = identityMatrix(); }
void C4JRender::MatrixTranslate(float x, float y, float z) {
    Mat4 m = identityMatrix(); m[12] = x; m[13] = y; m[14] = z;
    g_matrices[g_matrixMode] = multiply(g_matrices[g_matrixMode], m);
}
void C4JRender::MatrixRotate(float angle, float x, float y, float z) {
    const float length = std::sqrt(x*x + y*y + z*z);
    if (length <= 0.000001f) return;
    x /= length; y /= length; z /= length;
    const float c = std::cos(angle), s = std::sin(angle), t = 1.0f - c;
    Mat4 m = identityMatrix();
    m[0] = t*x*x + c;     m[4] = t*x*y - s*z; m[8] = t*x*z + s*y;
    m[1] = t*x*y + s*z;   m[5] = t*y*y + c;   m[9] = t*y*z - s*x;
    m[2] = t*x*z - s*y;   m[6] = t*y*z + s*x; m[10] = t*z*z + c;
    g_matrices[g_matrixMode] = multiply(g_matrices[g_matrixMode], m);
}
void C4JRender::MatrixScale(float x, float y, float z) {
    Mat4 m = identityMatrix(); m[0] = x; m[5] = y; m[10] = z;
    g_matrices[g_matrixMode] = multiply(g_matrices[g_matrixMode], m);
}
void C4JRender::MatrixPerspective(float fovy, float aspect, float zNear, float zFar) {
    if (aspect == 0.0f || zNear == zFar) return;
    const float f = 1.0f / std::tan(fovy * 0.008726646259971648f);
    Mat4 m{};
    m[0] = f / aspect; m[5] = f;
    m[10] = (zFar + zNear) / (zNear - zFar);
    m[11] = -1.0f;
    m[14] = (2.0f * zFar * zNear) / (zNear - zFar);
    g_matrices[LEGACY_PROJECTION] = m;
}
void C4JRender::MatrixOrthogonal(float left, float right, float bottom, float top, float zNear, float zFar) {
    if (right == left || top == bottom || zFar == zNear) return;
    Mat4 m = identityMatrix();
    m[0] = 2.0f / (right - left);
    m[5] = 2.0f / (top - bottom);
    m[10] = -2.0f / (zFar - zNear);
    m[12] = -(right + left) / (right - left);
    m[13] = -(top + bottom) / (top - bottom);
    m[14] = -(zFar + zNear) / (zFar - zNear);
    g_matrices[LEGACY_PROJECTION] = m;
}
void C4JRender::MatrixPop() {
    std::vector<Mat4>& stack = g_matrixStacks[g_matrixMode];
    if (!stack.empty()) { g_matrices[g_matrixMode] = stack.back(); stack.pop_back(); }
}
void C4JRender::MatrixPush() { g_matrixStacks[g_matrixMode].push_back(g_matrices[g_matrixMode]); }
void C4JRender::MatrixMult(float* matrix) {
    if (matrix == nullptr) return;
    Mat4 m{}; std::memcpy(m.data(), matrix, sizeof(float) * 16);
    g_matrices[g_matrixMode] = multiply(g_matrices[g_matrixMode], m);
}
const float* C4JRender::MatrixGet(int type) {
    if (type == LEGACY_PROJECTION) return g_matrices[LEGACY_PROJECTION].data();
    if (type == LEGACY_TEXTURE_MATRIX) return g_matrices[LEGACY_TEXTURE_MATRIX].data();
    return g_matrices[LEGACY_MODELVIEW].data();
}
void C4JRender::Set_matrixDirty() {}

void C4JRender::Initialise(ID3D11Device*, IDXGISwapChain*) { InitialiseContext(); }
void C4JRender::InitialiseContext() { ensureRenderer(); }
void C4JRender::StartFrame() {}
void C4JRender::DoScreenGrabOnNextPresent() {}
void C4JRender::Present() {}
void C4JRender::Clear(int flags, D3D11_RECT*) {
    GLbitfield mask = 0;
    if (flags & CLEAR_COLOUR_FLAG) mask |= GL_COLOR_BUFFER_BIT;
    if (flags & CLEAR_DEPTH_FLAG) mask |= GL_DEPTH_BUFFER_BIT;
    glClearColor(g_clearColour[0], g_clearColour[1], g_clearColour[2], g_clearColour[3]);
    glClear(mask);
}
void C4JRender::SetClearColour(const float colourRGBA[4]) {
    if (colourRGBA) std::memcpy(g_clearColour, colourRGBA, sizeof(g_clearColour));
}
bool C4JRender::IsWidescreen() { return g_width >= g_height; }
bool C4JRender::IsHiDef() { return true; }
void C4JRender::CaptureThumbnail(ImageFileBuffer* output) {
    if (output) { output->m_pBuffer = nullptr; output->m_bufferSize = 0; }
}
void C4JRender::CaptureScreen(ImageFileBuffer* output, XSOCIAL_PREVIEWIMAGE* preview) {
    if (output) { output->m_pBuffer = nullptr; output->m_bufferSize = 0; }
    if (preview) { preview->pBytes = nullptr; preview->Pitch = preview->Width = preview->Height = 0; }
}
void C4JRender::BeginConditionalSurvey(int) {}
void C4JRender::EndConditionalSurvey() {}
void C4JRender::BeginConditionalRendering(int) {}
void C4JRender::EndConditionalRendering() {}

void C4JRender::DrawVertices(ePrimitiveType primitive, int count, void* data, eVertexType type, ePixelShaderType shader) {
    captureOrDraw(static_cast<int>(primitive), count, data, static_cast<int>(type), static_cast<int>(shader));
}
void C4JRender::DrawVertexBuffer(ePrimitiveType, int, ID3D11Buffer*, eVertexType, ePixelShaderType) {}

void C4JRender::CBuffLockStaticCreations() {}
int C4JRender::CBuffCreate(int count) {
    if (count <= 0) return 0;
    const int first = g_nextList;
    for (int i = 0; i < count; ++i) g_lists.emplace(g_nextList++, std::vector<DrawCall>{});
    return first;
}
void C4JRender::CBuffDelete(int first, int count) {
    for (int i = 0; i < count; ++i) g_lists.erase(first + i);
}
void C4JRender::CBuffStart(int index, bool) {
    auto found = g_lists.find(index);
    if (found == g_lists.end()) return;
    found->second.clear();
    g_captureList = index;
}
void C4JRender::CBuffClear(int index) {
    auto found = g_lists.find(index);
    if (found != g_lists.end()) found->second.clear();
}
int C4JRender::CBuffSize(int index) {
    auto found = g_lists.find(index);
    if (found == g_lists.end()) return 0;
    int count = 0;
    for (const DrawCall& draw : found->second) count += draw.count;
    return count;
}
void C4JRender::CBuffEnd() { g_captureList = -1; }
bool C4JRender::CBuffCall(int index, bool) {
    auto found = g_lists.find(index);
    if (found == g_lists.end()) return false;
    for (const DrawCall& draw : found->second) drawImmediate(draw);
    return true;
}
void C4JRender::CBuffTick() {}
void C4JRender::CBuffDeferredModeStart() {}
void C4JRender::CBuffDeferredModeEnd() {}

int C4JRender::TextureCreate() {
    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    return static_cast<int>(texture);
}
void C4JRender::TextureFree(int index) {
    if (index <= 0) return;
    GLuint texture = static_cast<GLuint>(index);
    glDeleteTextures(1, &texture);
    if (g_boundTexture == texture) { g_boundTexture = 0; g_textureEnabled = false; }
}
void C4JRender::TextureBind(int index) {
    g_textureEnabled = index >= 0;
    g_boundTexture = index >= 0 ? static_cast<GLuint>(index) : 0;
    glBindTexture(GL_TEXTURE_2D, g_boundTexture);
}
void C4JRender::TextureBindVertex(int index) { TextureBind(index); }
void C4JRender::TextureSetTextureLevels(int levels) { g_textureLevels = levels > 0 ? levels : 1; }
int C4JRender::TextureGetTextureLevels() { return g_textureLevels; }
void C4JRender::TextureData(int width, int height, void* data, int level, eTextureFormat) {
    if (width <= 0 || height <= 0) return;
    glTexImage2D(GL_TEXTURE_2D, level, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
}
void C4JRender::TextureDataUpdate(int x, int y, int width, int height, void* data, int level) {
    if (width <= 0 || height <= 0) return;
    glTexSubImage2D(GL_TEXTURE_2D, level, x, y, width, height, GL_RGBA, GL_UNSIGNED_BYTE, data);
}
void C4JRender::TextureSetParam(int param, int value) {
    GLenum name = 0;
    if (param == LEGACY_TEXTURE_MIN_FILTER) name = GL_TEXTURE_MIN_FILTER;
    else if (param == LEGACY_TEXTURE_MAG_FILTER) name = GL_TEXTURE_MAG_FILTER;
    else if (param == LEGACY_TEXTURE_WRAP_S) name = GL_TEXTURE_WRAP_S;
    else if (param == LEGACY_TEXTURE_WRAP_T) name = GL_TEXTURE_WRAP_T;
    if (name == 0) return;

    GLint mapped = GL_NEAREST;
    if (param == LEGACY_TEXTURE_MIN_FILTER || param == LEGACY_TEXTURE_MAG_FILTER) {
        mapped = value == 1 ? GL_LINEAR : GL_NEAREST;
    } else {
        mapped = value == 1 ? GL_REPEAT : GL_CLAMP_TO_EDGE;
    }
    glTexParameteri(GL_TEXTURE_2D, name, mapped);
}
void C4JRender::TextureDynamicUpdateStart() {}
void C4JRender::TextureDynamicUpdateEnd() {}
HRESULT C4JRender::LoadTextureData(const char*, D3DXIMAGE_INFO* info, int** output) {
    if (info) { info->Width = 0; info->Height = 0; }
    if (output) *output = nullptr;
    return static_cast<HRESULT>(-1);
}
HRESULT C4JRender::LoadTextureData(BYTE*, DWORD, D3DXIMAGE_INFO* info, int** output) {
    if (info) { info->Width = 0; info->Height = 0; }
    if (output) *output = nullptr;
    return static_cast<HRESULT>(-1);
}
HRESULT C4JRender::SaveTextureData(const char*, D3DXIMAGE_INFO*, int*) { return static_cast<HRESULT>(-1); }
HRESULT C4JRender::SaveTextureDataToMemory(void*, int, int* length, int, int, int*) {
    if (length) *length = 0;
    return static_cast<HRESULT>(-1);
}
void C4JRender::TextureGetStats() {}
ID3D11ShaderResourceView* C4JRender::TextureGetTexture(int) { return nullptr; }

void C4JRender::StateSetColour(float r, float g, float b, float a) {
    g_colour[0] = r; g_colour[1] = g; g_colour[2] = b; g_colour[3] = a;
}
void C4JRender::StateSetDepthMask(bool enable) { glDepthMask(enable ? GL_TRUE : GL_FALSE); }
void C4JRender::StateSetBlendEnable(bool enable) {
    if (enable) glEnable(GL_BLEND); else glDisable(GL_BLEND);
}
void C4JRender::StateSetBlendFunc(int src, int dst) { glBlendFunc(mapBlendFactor(src), mapBlendFactor(dst)); }
void C4JRender::StateSetBlendFactor(unsigned int) {}
void C4JRender::StateSetAlphaFunc(int, float value) { g_alphaCutoff = value; }
void C4JRender::StateSetDepthFunc(int value) { glDepthFunc(mapDepthFunction(value)); }
void C4JRender::StateSetFaceCull(bool enable) {
    if (enable) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
}
void C4JRender::StateSetFaceCullCW(bool clockwise) { glFrontFace(clockwise ? GL_CW : GL_CCW); }
void C4JRender::StateSetLineWidth(float width) { glLineWidth(width > 0.0f ? width : 1.0f); }
void C4JRender::StateSetWriteEnable(bool red, bool green, bool blue, bool alpha) {
    glColorMask(red ? GL_TRUE : GL_FALSE, green ? GL_TRUE : GL_FALSE,
                blue ? GL_TRUE : GL_FALSE, alpha ? GL_TRUE : GL_FALSE);
}
void C4JRender::StateSetDepthTestEnable(bool enable) {
    if (enable) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
}
void C4JRender::StateSetAlphaTestEnable(bool enable) { g_alphaTest = enable; }
void C4JRender::StateSetDepthSlopeAndBias(float slope, float bias) {
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(slope, bias);
}
void C4JRender::StateSetFogEnable(bool) {}
void C4JRender::StateSetFogMode(int) {}
void C4JRender::StateSetFogNearDistance(float) {}
void C4JRender::StateSetFogFarDistance(float) {}
void C4JRender::StateSetFogDensity(float) {}
void C4JRender::StateSetFogColour(float, float, float) {}
void C4JRender::StateSetLightingEnable(bool) {}
void C4JRender::StateSetVertexTextureUV(float, float) {}
void C4JRender::StateSetLightColour(int, float, float, float) {}
void C4JRender::StateSetLightAmbientColour(float, float, float) {}
void C4JRender::StateSetLightDirection(int, float, float, float) {}
void C4JRender::StateSetLightEnable(int, bool) {}
void C4JRender::StateSetViewport(eViewportType type) {
    int x = 0, y = 0, width = g_width, height = g_height;
    switch (type) {
        case VIEWPORT_TYPE_SPLIT_TOP: height /= 2; break;
        case VIEWPORT_TYPE_SPLIT_BOTTOM: height /= 2; y = height; break;
        case VIEWPORT_TYPE_SPLIT_LEFT: width /= 2; break;
        case VIEWPORT_TYPE_SPLIT_RIGHT: width /= 2; x = width; break;
        case VIEWPORT_TYPE_QUADRANT_TOP_LEFT: width /= 2; height /= 2; break;
        case VIEWPORT_TYPE_QUADRANT_TOP_RIGHT: width /= 2; height /= 2; x = width; break;
        case VIEWPORT_TYPE_QUADRANT_BOTTOM_LEFT: width /= 2; height /= 2; y = height; break;
        case VIEWPORT_TYPE_QUADRANT_BOTTOM_RIGHT: width /= 2; height /= 2; x = width; y = height; break;
        default: break;
    }
    glViewport(x, y, width > 0 ? width : 1, height > 0 ? height : 1);
}
void C4JRender::StateSetEnableViewportClipPlanes(bool) {}
void C4JRender::StateSetTexGenCol(int, float, float, float, float, bool) {}
void C4JRender::StateSetStencil(int function, uint8_t reference, uint8_t readMask, uint8_t writeMask) {
    glEnable(GL_STENCIL_TEST);
    glStencilFunc(mapDepthFunction(function), reference, readMask);
    glStencilMask(writeMask);
}
void C4JRender::StateSetForceLOD(int) {}
void C4JRender::BeginEvent(LPCWSTR) {}
void C4JRender::EndEvent() {}
void C4JRender::Suspend() { g_suspended = true; }
bool C4JRender::Suspended() { return g_suspended; }
void C4JRender::Resume() { g_suspended = false; }

extern "C" void MclgAndroidRendererResize(int width, int height) {
    g_width = width > 0 ? width : 1;
    g_height = height > 0 ? height : 1;
    glViewport(0, 0, g_width, g_height);
}
extern "C" void MclgAndroidRendererInitialize() {
    g_width = 1;
    g_height = 1;
    RenderManager.InitialiseContext();
}
extern "C" void MclgAndroidRendererBeginFrame() {
    RenderManager.StartFrame();
}
