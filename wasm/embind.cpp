#include <vector>
#include <string>
#include <exception>

#include <ebsynth.h>
#include <jzq.h>

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

namespace
{

    int evalNumChannels(const unsigned char *data, const int numPixels)
    {
        bool isGray = true;
        bool hasAlpha = false;

        for (int xy = 0; xy < numPixels; xy++)
        {
            const unsigned char r = data[xy * 4 + 0];
            const unsigned char g = data[xy * 4 + 1];
            const unsigned char b = data[xy * 4 + 2];
            const unsigned char a = data[xy * 4 + 3];

            if (!(r == g && g == b))
            {
                isGray = false;
            }
            if (a < 255)
            {
                hasAlpha = true;
            }
        }

        const int numChannels = (isGray ? 1 : 3) + (hasAlpha ? 1 : 0);

        return numChannels;
    }

    V2i pyramidLevelSize(const V2i &sizeBase, const int level)
    {
        return V2i(V2f(sizeBase) * std::pow(2.0f, -float(level)));
    }

} // anonymous namespace

struct ebsynthRunResult
{
    string nnfData;
    string imageData;
};

struct nativeEbsynthGuide
{
    double weight;
    int sourceWidth;
    int sourceHeight;
    vector<uint8_t> sourceData;
    int targetWidth;
    int targetHeight;
    vector<uint8_t> targetData;
    int numChannels;
};

inline void validateStyle(val style) {
    // Check that `style` is an ImageData. Throw if it isn't
    if (!style.instanceof(val::global("ImageData"))) {
        throw std::runtime_error("style must be an ImageData");
    }
}

/**
 * @brief Validates that the given emscripten::val contains an array of valid guide objects
 * 
 * A valid guide object is an object with the following properties:
 * - source: ImageData
 * - target: ImageData
 * - weight: number
 * 
 * @param guides 
 */
inline void validateGuides(val guides) {
    // Check that `guides` is an array. Throw if it isn't
    if (!guides.isArray()) {
        throw std::runtime_error("guides must be an array");
    }
    // It's an array, so no go through each element and validate it
    for (size_t i = 0; i < guides["length"].as<size_t>(); i++) {
        val guide = guides[i];

        auto source = guide["source"];
        if (source.isUndefined()) {
            throw std::runtime_error("guide " + std::to_string(i) + " must have a source property");
        }
        if (!source.instanceof(val::global("ImageData"))) {
            throw std::runtime_error("guide " + std::to_string(i) + " source property must be an ImageData");
        }

        // Check that the guide has a target property. Throw if it doesn't
        auto target = guide["target"];
        if (target.isUndefined()) {
            throw std::runtime_error("guide " + std::to_string(i) + " must have a target property");
        }
        if (!target.instanceof(val::global("ImageData"))) {
            throw std::runtime_error("guide " + std::to_string(i) + " target property must be an ImageData");
        }

        // Check that the guide has a weight property. Throw if it doesn't
        if (!guide["weight"].isNumber()) {
            throw std::runtime_error("guide " + std::to_string(i) + " must have a weight property");
        }
    }
}

/**
 * @brief Runs ebsynth with the given inputs
 *
 * @param[in] style An ImageData object containing the style image
 * @param[in] guides An array of objects containing the guide images and weights. See
 *            validateGuides for more info
 * @param[in] uniformityWeight The uniformity parameter for EbSynth
 * @param[in] patchSize The size of patches to search for when running PatchMatch. Larger values
 *            incur a performance hit, and may result in less detail in the output. Usually a patch
 *            size between 3 and 7 is good. Value must be odd.
 * @param[in] numPyramidLevels The number of pyramid levels to use. The default value of -1 will
 *            use the maximum number of levels possible for the given input resolution. The maximum
 *            number of levels is 32.
 * @param[in] numSearchVoteIters The number of search vote iterations to run. More iterations will
 *            result in a better output, but will take longer to compute. Pass -1 to use a default
 * @param[in] numPatchMatchIters The number of PatchMatch iterations to run. More iterations will
 *            result in a better output, but will take longer to compute. Pass -1 to use a default
 * @param[in] stopThreshold The stop threshold for PatchMatch. Pass -1 to use a default
 * @param[in] extraPass3x3 Whether to take an extra 3x3 pass to improve results. Often unnecessary
 *
 * @return ebsynthRunResult containing the NNF and output image data
 */
static ebsynthRunResult ebsynthRunEmbind(
    val style, // ImageData
    val guides, // Array of guide objects
    double uniformityWeight,
    int patchSize,
    int numPyramidLevels,
    int numSearchVoteIters,
    int numPatchMatchIters,
    int stopThreshold,
    bool extraPass3x3)
{
    validateStyle(style);
    validateGuides(guides);

    if (numSearchVoteIters == -1) {
        numSearchVoteIters = 6;
    }
    if (numPatchMatchIters == -1) {
        numPatchMatchIters = 4;
    }
    if (stopThreshold == -1) {
        stopThreshold = 5;
    }

    int sourceWidth = style["width"].as<int>();
    int sourceHeight = style["height"].as<int>();
    val pixelsTyped = style["data"];
    // Extract the raw data from the ImageData object
    vector<uint8_t> sourceStyleRaw(pixelsTyped["length"].as<size_t>());
    for (size_t i = 0; i < sourceStyleRaw.size(); i++)
    {
        sourceStyleRaw[i] = pixelsTyped[i].as<uint8_t>();
    }
    const int numStyleChannelsTotal = evalNumChannels(sourceStyleRaw.data(), sourceWidth * sourceHeight);
    // Based on number of practical channels, populate a condensed data vector
    std::vector<unsigned char> sourceStyle(sourceWidth * sourceHeight * numStyleChannelsTotal);
    for (int xy = 0; xy < sourceWidth * sourceHeight; xy++)
    {
        if (numStyleChannelsTotal > 0)
        {
            sourceStyle[xy * numStyleChannelsTotal + 0] = sourceStyle[xy * 4 + 0];
        }
        if (numStyleChannelsTotal == 2)
        {
            sourceStyle[xy * numStyleChannelsTotal + 1] = sourceStyle[xy * 4 + 3];
        }
        else if (numStyleChannelsTotal > 1)
        {
            sourceStyle[xy * numStyleChannelsTotal + 1] = sourceStyle[xy * 4 + 1];
        }
        if (numStyleChannelsTotal > 2)
        {
            sourceStyle[xy * numStyleChannelsTotal + 2] = sourceStyle[xy * 4 + 2];
        }
        if (numStyleChannelsTotal > 3)
        {
            sourceStyle[xy * numStyleChannelsTotal + 3] = sourceStyle[xy * 4 + 3];
        }
    }

    // Load guide data
    int targetWidth = 0;
    int targetHeight = 0;
    int numGuideChannelsTotal = 0;
    auto numGuides = guides["length"].as<size_t>();
    vector<nativeEbsynthGuide> nativeGuides(numGuides);
    for (int i = 0; i < numGuides; i++)
    {
        // Assume guides have already been validated
        auto guide = guides[i];
        auto guideSource = guide["source"]; // ImageData
        auto guideTarget = guide["target"]; // ImageData
        auto guideWeight = guide["weight"]; // number

        auto _sourceWidth = guideSource["width"].as<int>();
        auto _sourceHeight = guideSource["height"].as<int>();
        auto _targetWidth = guideTarget["width"].as<int>();
        auto _targetHeight = guideTarget["height"].as<int>();

        val sourcePixelsTyped = guideSource["data"];
        vector<uint8_t> sourceGuideData(sourcePixelsTyped["length"].as<size_t>());
        for (size_t i = 0; i < sourceGuideData.size(); i++)
        {
            sourceGuideData[i] = sourcePixelsTyped[i].as<uint8_t>();
        }
        val targetPixelsTyped = guideTarget["data"];
        vector<uint8_t> targetGuideData(targetPixelsTyped["length"].as<size_t>());
        for (size_t i = 0; i < targetGuideData.size(); i++)
        {
            targetGuideData[i] = targetPixelsTyped[i].as<uint8_t>();
        }

        if (_sourceWidth != sourceWidth || _sourceHeight != sourceHeight)
        {
            throw std::runtime_error("source guide " + std::to_string(i) + " doesn't match the resolution of source style");
        }
        if (i > 0 && (_targetWidth != targetWidth || _targetHeight != targetHeight))
        {
            throw std::runtime_error("target guide " + std::to_string(i) + " doesn't match the resolution of earlier guides guide");
        }
        else if (i == 0)
        {
            targetWidth = _targetWidth;
            targetHeight = _targetHeight;
        }

        auto numChannels = std::max(evalNumChannels(sourceGuideData.data(), _sourceWidth * _sourceHeight),
                                    evalNumChannels(targetGuideData.data(), _targetWidth * _targetHeight));

        numGuideChannelsTotal += numChannels;

        nativeGuides[i].weight = guideWeight.as<double>();
        nativeGuides[i].sourceWidth = _sourceWidth;
        nativeGuides[i].sourceHeight = _sourceHeight;
        nativeGuides[i].sourceData = sourceGuideData;
        nativeGuides[i].targetWidth = _targetWidth;
        nativeGuides[i].targetHeight = _targetHeight;
        nativeGuides[i].targetData = targetGuideData;
        nativeGuides[i].numChannels = numChannels;
    }

    if (numStyleChannelsTotal > EBSYNTH_MAX_STYLE_CHANNELS)
    {
        throw std::runtime_error("too many style channels (" + std::to_string(numStyleChannelsTotal) + "), maximum number is " + std::to_string(EBSYNTH_MAX_STYLE_CHANNELS));
    }
    if (numGuideChannelsTotal > EBSYNTH_MAX_GUIDE_CHANNELS)
    {
        throw std::runtime_error("too many guide channels (" + std::to_string(numGuideChannelsTotal) + "), maximum number is " + std::to_string(EBSYNTH_MAX_GUIDE_CHANNELS));
    }

    // Now that we've counted the number of channels, and checked that the resolutions match, we
    // can populate the condensed data vectors
    std::vector<uint8_t> sourceGuides(sourceWidth * sourceHeight * numGuideChannelsTotal);
    for (int xy = 0; xy < sourceWidth * sourceHeight; xy++)
    {
        int c = 0;
        for (int i = 0; i < numGuides; i++)
        {
            const int numChannels = nativeGuides[i].numChannels;

            if (numChannels > 0)
            {
                sourceGuides[xy * numGuideChannelsTotal + c + 0] = nativeGuides[i].sourceData[xy * 4 + 0];
            }
            if (numChannels == 2)
            {
                sourceGuides[xy * numGuideChannelsTotal + c + 1] = nativeGuides[i].sourceData[xy * 4 + 3];
            }
            else if (numChannels > 1)
            {
                sourceGuides[xy * numGuideChannelsTotal + c + 1] = nativeGuides[i].sourceData[xy * 4 + 1];
            }
            if (numChannels > 2)
            {
                sourceGuides[xy * numGuideChannelsTotal + c + 2] = nativeGuides[i].sourceData[xy * 4 + 2];
            }
            if (numChannels > 3)
            {
                sourceGuides[xy * numGuideChannelsTotal + c + 3] = nativeGuides[i].sourceData[xy * 4 + 3];
            }

            c += numChannels;
        }
    }

    std::vector<uint8_t> targetGuides(targetWidth * targetHeight * numGuideChannelsTotal);
    for (int xy = 0; xy < targetWidth * targetHeight; xy++)
    {
        int c = 0;
        for (int i = 0; i < numGuides; i++)
        {
            const int numChannels = nativeGuides[i].numChannels;

            if (numChannels > 0)
            {
                targetGuides[xy * numGuideChannelsTotal + c + 0] = nativeGuides[i].targetData[xy * 4 + 0];
            }
            if (numChannels == 2)
            {
                targetGuides[xy * numGuideChannelsTotal + c + 1] = nativeGuides[i].targetData[xy * 4 + 3];
            }
            else if (numChannels > 1)
            {
                targetGuides[xy * numGuideChannelsTotal + c + 1] = nativeGuides[i].targetData[xy * 4 + 1];
            }
            if (numChannels > 2)
            {
                targetGuides[xy * numGuideChannelsTotal + c + 2] = nativeGuides[i].targetData[xy * 4 + 2];
            }
            if (numChannels > 3)
            {
                targetGuides[xy * numGuideChannelsTotal + c + 3] = nativeGuides[i].targetData[xy * 4 + 3];
            }

            c += numChannels;
        }
    }

    // We only have one style for now, which consists of 1 or more channels. Normalize the default
    // weight by the number of channels, so the sum is 1.0
    const float defaultStyleWeight = 1.0f;
    vector<float> styleWeights(numStyleChannelsTotal, defaultStyleWeight);
    for (int i = 0; i < numStyleChannelsTotal; i++)
    {
        styleWeights[i] = defaultStyleWeight / float(numStyleChannelsTotal);
    }

    // For each guide, ensure we have a default weight value.
    for (int i = 0; i < numGuides; i++)
    {
        if (nativeGuides[i].weight < 0)
        {
            nativeGuides[i].weight = 1.0f / float(numGuides);
        }
    }
    // Now, populate the guide weights vector, which is a flat array of weights for each channel
    std::vector<float> guideWeights(numGuideChannelsTotal);
    {
        int c = 0;
        for (int i = 0; i < numGuides; i++)
        {
            const int numChannels = nativeGuides[i].numChannels;

            for (int j = 0; j < numChannels; j++)
            {
                guideWeights[c + j] = nativeGuides[i].weight / float(numChannels);
            }

            c += numChannels;
        }
    }

    int maxPyramidLevels = 0;
    for (int level = 32; level >= 0; level--)
    {
        if (min(pyramidLevelSize(std::min(V2i(sourceWidth, sourceHeight), V2i(targetWidth, targetHeight)), level)) >= (2 * patchSize + 1))
        {
            maxPyramidLevels = level + 1;
            break;
        }
    }

    if (numPyramidLevels == -1)
    {
        numPyramidLevels = maxPyramidLevels;
    }
    numPyramidLevels = std::min(numPyramidLevels, maxPyramidLevels);

    vector<int> numSearchVoteItersPerLevel(numPyramidLevels);
    vector<int> numPatchMatchItersPerLevel(numPyramidLevels);
    vector<int> stopThresholdPerLevel(numPyramidLevels);
    for (int i = 0; i < numPyramidLevels; ++i)
    {
        numSearchVoteItersPerLevel[i] = numSearchVoteIters;
        numPatchMatchItersPerLevel[i] = numPatchMatchIters;
        stopThresholdPerLevel[i] = stopThreshold;
    }

    ebsynthRunResult result;
    result.nnfData.resize(targetWidth * targetHeight * 2);
    result.imageData.resize(targetWidth * targetHeight * numStyleChannelsTotal);

    ebsynthRun(
        wasm_backend,
        numStyleChannelsTotal,
        numGuideChannelsTotal,
        sourceWidth,
        sourceHeight,
        sourceStyle.data(),
        sourceGuides.data(),
        targetWidth,
        targetHeight,
        targetGuides.data(),
        nullptr,
        styleWeights.data(),
        guideWeights.data(),
        uniformityWeight,
        patchSize,
        EBSYNTH_VOTEMODE_PLAIN,
        numPyramidLevels,
        numSearchVoteItersPerLevel.data(),
        numPatchMatchItersPerLevel.data(),
        stopThresholdPerLevel.data(),
        extraPass3x3,
        result.nnfData.data(),
        result.imageData.data());

    return result;
}

int main(int argc, char **argv)
{
    emscripten_exit_with_live_runtime();
}

namespace emscripten {
template<typename T>
class_<std::vector<T>> register_vector_extended(const char* name) {
    auto new_vector = register_vector<T>(name);
    EM_ASM({
        const className = UTF8ToString($0);
        Module[className].prototype[Symbol.iterator] = function* VectorIterator() {
            for (let index = 0; index < this.size(); index++) {
                yield this.get(index)
            }
        };
        Module[className].prototype.toArray = function toArray() {
            return Array.from(this)
        };
        Module[className].prototype.forEach = function forEach(fn) {
            for (let index = 0; index < this.size(); index++) {
                fn(this.get(index), index, this);
            }
        };
    }, name);
    return new_vector;
}
}

EMSCRIPTEN_BINDINGS(ebsynth)
{
    value_object<ebsynthRunResult>("ebsynthRunResult")
        .field("nnfData", &ebsynthRunResult::nnfData)
        .field("imageData", &ebsynthRunResult::imageData);

    function("ebsynthRun", &ebsynthRunEmbind);
}