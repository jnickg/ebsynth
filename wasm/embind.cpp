#include <vector>
#include <string>

#include <ebsynth.h>

#ifndef __EMSCRIPTEN__
#error "This file should only be included when compiling for Emscripten"
#endif

#include <emscripten/emscripten.h>
#include <emscripten/bind.h>

using std::string;
using std::vector;
using namespace emscripten;

static inline constexpr int wasm_backend = EBSYNTH_BACKEND_CPU;

namespace emscripten
{
    namespace internal
    {

        // This will automatically convert a JS array to a std::vector (for C++ function parameters) and a
        // std::vector to a JS array (for C++ return values) without having to mess with register_vector,
        // as long as the T type in std::vector<T> has bindings defined.

        // NOTE that this does not work well for rich objects, as changes to the returned/passed objects
        // won't latch on the other side of the wasm boundary. However, it works well for value types for
        // which the JS and C++ sides don't need to mutate the objects (like passing an array of numbers)

        template <typename T, typename Allocator>
        struct BindingType<std::vector<T, Allocator>>
        {
            using ValBinding = BindingType<val>;
            using WireType = ValBinding::WireType;

            static WireType toWireType(const std::vector<T, Allocator> &vec)
            {
                return ValBinding::toWireType(val::array(vec));
            }

            static std::vector<T, Allocator> fromWireType(WireType value)
            {
                return vecFromJSArray<T>(ValBinding::fromWireType(value));
            }
        };

        template <typename T>
        struct TypeID<T,
                      typename std::enable_if_t<std::is_same<
                          typename Canonicalized<T>::type,
                          std::vector<typename Canonicalized<T>::type::value_type,
                                      typename Canonicalized<T>::type::allocator_type>>::value>>
        {
            static constexpr TYPEID get() { return TypeID<val>::get(); }
        };

    } // namespace internal
} // namespace emscripten

struct ebsynthRunResult {
    string nnfData;
    string imageData;
};

static ebsynthRunResult ebsynthRunEmbind(int numStyleChannels,
                                         int numGuideChannels,
                                         int sourceWidth,
                                         int sourceHeight,
                                         string sourceStyleData,
                                         string sourceGuideData,
                                         int targetWidth,
                                         int targetHeight,
                                         string targetGuideData,
                                         string targetModulationData,
                                         vector<float> styleWeights,
                                         vector<float> guideWeights,
                                         float uniformityWeight,
                                         int patchSize,
                                         int voteMode,
                                         int numPyramidLevels,
                                         vector<int> numSearchVoteItersPerLevel,
                                         vector<int> numPatchMatchItersPerLevel,
                                         vector<int> stopThresholdPerLevel,
                                         bool extraPass3x3
) {
    ebsynthRunResult result;
    result.nnfData.resize(targetWidth * targetHeight * 2);
    result.imageData.resize(targetWidth * targetHeight * numStyleChannels);

    ebsynthRun(
        wasm_backend,
        numStyleChannels,
        numGuideChannels,
        sourceWidth,
        sourceHeight,
        sourceStyleData.data(),
        sourceGuideData.data(),
        targetWidth,
        targetHeight,
        targetGuideData.data(),
        targetModulationData.data(),
        styleWeights.data(),
        guideWeights.data(),
        uniformityWeight,
        patchSize,
        voteMode,
        numPyramidLevels,
        numSearchVoteItersPerLevel.data(),
        numPatchMatchItersPerLevel.data(),
        stopThresholdPerLevel.data(),
        extraPass3x3,
        result.nnfData.data(),
        result.imageData.data()
    );

    return result;
}

int main(int argc, char** argv) {
    emscripten_exit_with_live_runtime();
}

EMSCRIPTEN_BINDINGS(ebsynth) {
    value_object<ebsynthRunResult>("ebsynthRunResult")
        .field("nnfData", &ebsynthRunResult::nnfData)
        .field("imageData", &ebsynthRunResult::imageData);

    function("ebsynthRun", &ebsynthRunEmbind);
}