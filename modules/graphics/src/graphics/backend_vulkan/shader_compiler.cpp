#include "shader_compiler.hpp"

#include <fstream>

#include <glslang/Public/ShaderLang.h>
#include <glslang/SPIRV/GlslangToSpv.h>
#include <spirv_cross/spirv_glsl.hpp>
#include <core/module_manager.hpp>

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

namespace {

EShLanguage ToGlslangStage(ShaderStage stage) {
    switch (stage) {
        case ShaderStage::eVertex: return EShLangVertex;
        case ShaderStage::eTessellationControl: return EShLangTessControl;
        case ShaderStage::eTessellationEvaluation: return EShLangTessEvaluation;
        case ShaderStage::eGeometry: return EShLangGeometry;
        case ShaderStage::eFragment: return EShLangFragment;
        case ShaderStage::eCompute: return EShLangCompute;
    }
    Unreachable();
}

constexpr glslang::EShTargetClientVersion ToGlslangVulkanVersion() {
#if BISMUTH_VULKAN_VERSION_MINOR == 3
    return glslang::EShTargetVulkan_1_3;
#elif BISMUTH_VULKAN_VERSION_MINOR == 2
    return glslang::EShTargetVulkan_1_2;
#elif BISMUTH_VULKAN_VERSION_MINOR == 1
    return glslang::EShTargetVulkan_1_1;
#else
    return glslang::EShTargetVulkan_1_0;
#endif
}

constexpr glslang::EShTargetLanguageVersion ToGlslangSpvVersion() {
#if BISMUTH_VULKAN_VERSION_MINOR == 3
    return glslang::EShTargetSpv_1_6;
#elif BISMUTH_VULKAN_VERSION_MINOR == 2
    return glslang::EShTargetSpv_1_5;
#elif BISMUTH_VULKAN_VERSION_MINOR == 1
    return glslang::EShTargetSpv_1_3;
#else
    return glslang::EShTargetSpv_1_0;
#endif
}

const TBuiltInResource kDefaultTBuiltInResource = {
    .maxLights = 32,
    .maxClipPlanes = 6,
    .maxTextureUnits = 32,
    .maxTextureCoords = 32,
    .maxVertexAttribs = 64,
    .maxVertexUniformComponents = 4096,
    .maxVaryingFloats = 64,
    .maxVertexTextureImageUnits = 32,
    .maxCombinedTextureImageUnits = 80,
    .maxTextureImageUnits = 32,
    .maxFragmentUniformComponents = 4096,
    .maxDrawBuffers = 32,
    .maxVertexUniformVectors = 128,
    .maxVaryingVectors = 8,
    .maxFragmentUniformVectors = 16,
    .maxVertexOutputVectors = 16,
    .maxFragmentInputVectors = 15,
    .minProgramTexelOffset = -8,
    .maxProgramTexelOffset = 7,
    .maxClipDistances = 8,
    .maxComputeWorkGroupCountX = 65535,
    .maxComputeWorkGroupCountY = 65535,
    .maxComputeWorkGroupCountZ = 65535,
    .maxComputeWorkGroupSizeX = 1024,
    .maxComputeWorkGroupSizeY = 1024,
    .maxComputeWorkGroupSizeZ = 64,
    .maxComputeUniformComponents = 1024,
    .maxComputeTextureImageUnits = 16,
    .maxComputeImageUniforms = 8,
    .maxComputeAtomicCounters = 8,
    .maxComputeAtomicCounterBuffers = 1,
    .maxVaryingComponents = 60,
    .maxVertexOutputComponents = 64,
    .maxGeometryInputComponents = 64,
    .maxGeometryOutputComponents = 128,
    .maxFragmentInputComponents = 128,
    .maxImageUnits = 8,
    .maxCombinedImageUnitsAndFragmentOutputs = 8,
    .maxCombinedShaderOutputResources = 8,
    .maxImageSamples = 0,
    .maxVertexImageUniforms = 0,
    .maxTessControlImageUniforms = 0,
    .maxTessEvaluationImageUniforms = 0,
    .maxGeometryImageUniforms = 0,
    .maxFragmentImageUniforms = 8,
    .maxCombinedImageUniforms = 8,
    .maxGeometryTextureImageUnits = 16,
    .maxGeometryOutputVertices = 256,
    .maxGeometryTotalOutputComponents = 1024,
    .maxGeometryUniformComponents = 1024,
    .maxGeometryVaryingComponents = 64,
    .maxTessControlInputComponents = 128,
    .maxTessControlOutputComponents = 128,
    .maxTessControlTextureImageUnits = 16,
    .maxTessControlUniformComponents = 1024,
    .maxTessControlTotalOutputComponents = 4096,
    .maxTessEvaluationInputComponents = 128,
    .maxTessEvaluationOutputComponents = 128,
    .maxTessEvaluationTextureImageUnits = 16,
    .maxTessEvaluationUniformComponents = 1024,
    .maxTessPatchComponents = 120,
    .maxPatchVertices = 32,
    .maxTessGenLevel = 64,
    .maxViewports = 16,
    .maxVertexAtomicCounters = 0,
    .maxTessControlAtomicCounters = 0,
    .maxTessEvaluationAtomicCounters = 0,
    .maxGeometryAtomicCounters = 0,
    .maxFragmentAtomicCounters = 8,
    .maxCombinedAtomicCounters = 8,
    .maxAtomicCounterBindings = 1,
    .maxVertexAtomicCounterBuffers = 0,
    .maxTessControlAtomicCounterBuffers = 0,
    .maxTessEvaluationAtomicCounterBuffers = 0,
    .maxGeometryAtomicCounterBuffers = 0,
    .maxFragmentAtomicCounterBuffers = 1,
    .maxCombinedAtomicCounterBuffers = 1,
    .maxAtomicCounterBufferSize = 16384,
    .maxTransformFeedbackBuffers = 4,
    .maxTransformFeedbackInterleavedComponents = 64,
    .maxCullDistances = 8,
    .maxCombinedClipAndCullDistances = 8,
    .maxSamples = 4,
    .maxMeshOutputVerticesNV = 256,
    .maxMeshOutputPrimitivesNV = 512,
    .maxMeshWorkGroupSizeX_NV = 32,
    .maxMeshWorkGroupSizeY_NV = 1,
    .maxMeshWorkGroupSizeZ_NV = 1,
    .maxTaskWorkGroupSizeX_NV = 32,
    .maxTaskWorkGroupSizeY_NV = 1,
    .maxTaskWorkGroupSizeZ_NV = 1,
    .maxMeshViewCountNV = 4,
    .maxDualSourceDrawBuffersEXT = 1,
    .limits = {
        .nonInductiveForLoops = 1,
        .whileLoops = 1,
        .doWhileLoops = 1,
        .generalUniformIndexing = 1,
        .generalAttributeMatrixVectorIndexing = 1,
        .generalVaryingIndexing = 1,
        .generalSamplerIndexing = 1,
        .generalVariableIndexing = 1,
        .generalConstantMatrixVectorIndexing = 1,
    }
};

class GlslangIncluder : public glslang::TShader::Includer {
public:
    GlslangIncluder(const Vec<fs::path> &include_dirs)
        : include_dirs_stack_(include_dirs), input_include_dirs_size_(include_dirs.size()) {}

    IncludeResult* includeLocal(const char* headerName, const char* includerName, size_t inclusionDepth) override {
        include_dirs_stack_.resize(inclusionDepth + input_include_dirs_size_);
        if (inclusionDepth == 1) {
            include_dirs_stack_.back() = fs::path(includerName).parent_path();
        }

        for (auto it = include_dirs_stack_.rbegin(); it != include_dirs_stack_.rend(); ++it) {
            fs::path path = *it / headerName;
            if (fs::exists(path)) {
                include_dirs_stack_.push_back(path.parent_path());
                std::ifstream fin(path);
                fin.seekg(0, std::ios::end);
                size_t length = fin.tellg();
                fin.seekg(0, std::ios::beg);
                char *content = new char[length];
                fin.read(content, length);
                return new IncludeResult(path.string(), content, length, content);
            }
        }

        return nullptr;
    }

    IncludeResult* includeSystem(const char* headerName, const char* includerName, size_t inclusionDepth) override {
        return nullptr;
    }

    void releaseInclude(IncludeResult *result) override {
        if (result != nullptr) {
            delete[] static_cast<char *>(result->userData);
            delete result;
        }
    }

protected:
    size_t input_include_dirs_size_;
    Vec<fs::path> include_dirs_stack_;
};

}

ShaderCompilerVulkan::ShaderCompilerVulkan() {
    glslang::InitializeProcess();
}

ShaderCompilerVulkan::~ShaderCompilerVulkan() {
    glslang::FinalizeProcess();
}

Vec<uint8_t> ShaderCompilerVulkan::Compile(const fs::path &src_path, const std::string &entry, ShaderStage stage,
    const HashMap<std::string, std::string> &defines, const Vec<fs::path> &include_dirs) const {
    std::string src_filename = src_path.string();
    if (!fs::exists(src_path)) {
        BI_CRTICAL(ModuleManager::Get<GraphicsModule>()->Lgr(), "Shader file '{}' doesn't exist", src_filename);
    }

    // glslang may generate invalid SPIR-V 1.5/1.6 codes (see https://github.com/KhronosGroup/glslang/issues/2411)
    // hlsl -> spir-v 1.3 -> glsl -> spir-v 1.5/1.6 is used

    Vec<uint32_t> spv_temp = HlslToSpirv(src_filename, entry, stage, defines, include_dirs);

    std::string glsl_temp = SpirvToGlsl(spv_temp);

    Vec<uint32_t> spv_binary = GlslToSpirv(glsl_temp, entry, stage);
    Vec<uint8_t> spv_binary_bytes(spv_binary.size() * 4);
    memcpy(spv_binary_bytes.data(), spv_binary.data(), spv_binary.size() * 4);

    return spv_binary_bytes;
}

Vec<uint32_t> ShaderCompilerVulkan::HlslToSpirv(const std::string &src_filename, const std::string &entry,
    ShaderStage stage, const HashMap<std::string, std::string> &defines, const Vec<fs::path> &include_dirs) const {
    std::string shader_source;
    {
        std::ifstream fin(src_filename);
        fin.seekg(0, std::ios::end);
        size_t size = fin.tellg();
        shader_source.resize(size);
        fin.seekg(0, std::ios::beg);
        fin.read(shader_source.data(), size);
    }

    auto glslang_stage = ToGlslangStage(stage);
    glslang::TShader shader(glslang_stage);
    const char *shader_source_str = shader_source.c_str();
    shader.setStrings(&shader_source_str, 1);
    shader.setEnvInput(glslang::EShSourceHlsl, glslang_stage, glslang::EShClientVulkan, glslang::EShTargetVulkan_1_1);
    shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_1);
    shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_3);
    shader.setEntryPoint(entry.c_str());

    std::string preamble;
    for (const auto &[key, value] : defines) {
        preamble += "#define " + key + " " + value + "\n";
    }
    shader.setPreamble(preamble.c_str());

    GlslangIncluder includer(include_dirs);

    if (!shader.parse(&kDefaultTBuiltInResource, 100, false, EShMsgHlslLegalization, includer)) {
        const std::string log(shader.getInfoLog());
        const std::string debug_log(shader.getInfoDebugLog());
        BI_CRTICAL(ModuleManager::Get<GraphicsModule>()->Lgr(),
            "Failed to compile shader '{}' (entry point: '{}'), info:\n{}\n{}",
            src_filename, entry, log, debug_log);
    }

    glslang::TProgram program {};
    program.addShader(&shader);
    if (!program.link(EShMsgHlslLegalization)) {
        const std::string log(program.getInfoLog());
        const std::string debug_log(program.getInfoDebugLog());
        BI_CRTICAL(ModuleManager::Get<GraphicsModule>()->Lgr(),
            "Failed to link shader '{}' (entry point '{}'), info:\n{}\n{}",
            src_filename, entry, log, debug_log);
    }

    glslang::TIntermediate *intermediate = program.getIntermediate(glslang_stage);
    spv::SpvBuildLogger logger;
    glslang::SpvOptions options {};
    options.disableOptimizer = false;
    Vec<uint32_t> spv_binary;
    glslang::GlslangToSpv(*intermediate, spv_binary, &logger, &options);

    return spv_binary;
}

std::string ShaderCompilerVulkan::SpirvToGlsl(const Vec<uint32_t> &spv) const {
    spirv_cross::CompilerGLSL spv_to_glsl(spv.data(), spv.size());

    spirv_cross::CompilerGLSL::Options options {};
    options.vulkan_semantics = true;
    options.version = 460;
    spv_to_glsl.set_common_options(options);

    std::string glsl_src = spv_to_glsl.compile();
    return glsl_src;
}

Vec<uint32_t> ShaderCompilerVulkan::GlslToSpirv(const std::string &glsl_src, const std::string &entry,
    ShaderStage stage) const {
    auto glslang_stage = ToGlslangStage(stage);
    glslang::TShader shader(glslang_stage);
    const char *shader_source_str = glsl_src.c_str();
    shader.setStrings(&shader_source_str, 1);
    shader.setEnvInput(glslang::EShSourceGlsl, glslang_stage, glslang::EShClientVulkan, ToGlslangVulkanVersion());
    shader.setEnvClient(glslang::EShClientVulkan, ToGlslangVulkanVersion());
    shader.setEnvTarget(glslang::EShTargetSpv, ToGlslangSpvVersion());
    shader.setEntryPoint(entry.c_str());

    BI_ASSERT(shader.parse(&kDefaultTBuiltInResource, 100, false, EShMsgDefault));

    glslang::TProgram program {};
    program.addShader(&shader);
    BI_ASSERT(program.link(EShMsgDefault));

    glslang::TIntermediate *intermediate = program.getIntermediate(glslang_stage);
    spv::SpvBuildLogger logger;
    glslang::SpvOptions options {};
    options.disableOptimizer = false;
    Vec<uint32_t> spv_binary;
    glslang::GlslangToSpv(*intermediate, spv_binary, &logger, &options);

    return spv_binary;
}

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END
