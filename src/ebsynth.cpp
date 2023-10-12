// This software is in the public domain. Where that dedication is not
// recognized, you are granted a perpetual, irrevocable license to copy
// and modify this file as you see fit.

#include "ebsynth.h"
#include "ebsynth_cpu.h"
#include "ebsynth_cuda.h"

#include <cstdio>
#include <cmath>

EBSYNTH_API
void ebsynthRun(int    ebsynthBackend,
                int    numStyleChannels,
                int    numGuideChannels,
                int    sourceWidth,
                int    sourceHeight,
                void*  sourceStyleData,
                void*  sourceGuideData,
                int    targetWidth,
                int    targetHeight,
                void*  targetGuideData,
                void*  targetModulationData,
                float* styleWeights,
                float* guideWeights,
                float  uniformityWeight,
                int    patchSize,
                int    voteMode,
                int    numPyramidLevels,
                int*   numSearchVoteItersPerLevel,
                int*   numPatchMatchItersPerLevel,
                int*   stopThresholdPerLevel,
                int    extraPass3x3,
                void*  outputNnfData,
                void*  outputImageData)
{
  void (*backendDispatch)(int,int,int,int,void*,void*,int,int,void*,void*,float*,float*,float,int,int,int,int*,int*,int*,int,void*,void*) = 0;
  
  if      (ebsynthBackend==EBSYNTH_BACKEND_CPU ) { backendDispatch = ebsynthRunCpu;  }
  else if (ebsynthBackend==EBSYNTH_BACKEND_CUDA) { backendDispatch = ebsynthRunCuda; }
  else if (ebsynthBackend==EBSYNTH_BACKEND_AUTO) { backendDispatch = ebsynthBackendAvailableCuda() ? ebsynthRunCuda : ebsynthRunCpu; }
  
  if (backendDispatch!=0)
  {
    backendDispatch(numStyleChannels,
                    numGuideChannels,
                    sourceWidth,
                    sourceHeight,
                    sourceStyleData,
                    sourceGuideData,
                    targetWidth,
                    targetHeight,
                    targetGuideData,
                    targetModulationData,
                    styleWeights,
                    guideWeights,
                    uniformityWeight,
                    patchSize,
                    voteMode,
                    numPyramidLevels,
                    numSearchVoteItersPerLevel,
                    numPatchMatchItersPerLevel,
                    stopThresholdPerLevel,
                    extraPass3x3,
                    outputNnfData,
                    outputImageData);
  }
}

EBSYNTH_API
int ebsynthBackendAvailable(int ebsynthBackend)
{
  if      (ebsynthBackend==EBSYNTH_BACKEND_CPU ) { return ebsynthBackendAvailableCpu();  }
  else if (ebsynthBackend==EBSYNTH_BACKEND_CUDA) { return ebsynthBackendAvailableCuda(); }
  else if (ebsynthBackend==EBSYNTH_BACKEND_AUTO) { return ebsynthBackendAvailableCpu() || ebsynthBackendAvailableCuda(); }
  
  return 0;
}
