#include <optix.h>
#include <optix_device.h>
#include <optixu/optixu_math_namespace.h>
#include "random.h"

#include "Payloads.h"
#include "Config.h"
#include "Light.h"

using namespace optix;

rtBuffer<float3, 2> resultBuffer; // used to store the render result

rtDeclareVariable(rtObject, root, , ); // Optix graph

rtDeclareVariable(uint2, launchIndex, rtLaunchIndex, ); // a 2d index (x, y)

rtDeclareVariable(int1, frameID, , );

rtBuffer<Config> config; // Config

rtDeclareVariable(int, samples_per_pixel, , );

rtDeclareVariable(uint, next_event_est, , );

RT_PROGRAM void generateRays()
{
    size_t2 resultSize = resultBuffer.size();
    unsigned int index = launchIndex.x * resultSize.y + launchIndex.y;
    unsigned int seed = tea<16>(index * frameID.x, 0);
    Config cf = config[0];
    float3 result = make_float3(0.f);

    // Compute the ray direction
    float2 xy = make_float2(launchIndex);
    xy.x += frameID.x == 1 ? 0.5f : rnd(seed);
    xy.y += frameID.x == 1 ? 0.5f : rnd(seed);
    float2 ab = cf.tanHFov * (xy - cf.hSize) / cf.hSize;
    float3 dir = normalize(ab.x * cf.u + ab.y * cf.v - cf.w); // ray direction
    float3 origin = cf.eye; // ray origin

    Payload payload;
    int i = 0;

    //if (cf.next_event_est) cf.maxDepth--;

    //rtPrintf("depth is: %d\n", cf.maxDepth); 
    //rtPrintf("spp is: %d\n", samples_per_pixel); 
    // Iteratively trace rays (recursion is very expensive on GPU)
    for (int j = 0; j < samples_per_pixel; ++j) {
        // Prepare new payload for each sample
        payload.radiance = make_float3(.0f);
        payload.throughput = make_float3(1.0f);
        payload.depth = 0;
        payload.done = false;
        // jitter rays entering pixel
        xy = make_float2(launchIndex);
        xy.x += (j == 0) ? 0.5f : rnd(seed);
        xy.y += (j == 0) ? 0.5f : rnd(seed);
        ab = cf.tanHFov * (xy - cf.hSize) / cf.hSize;
        origin = cf.eye;
        dir = normalize(ab.x * cf.u + ab.y * cf.v - cf.w); // ray direction

        do
        {
            payload.seed = tea<16>(index * frameID.x, i++);

            // Trace a ray (or primary ray)
            Ray ray = make_Ray(origin, dir, 0, cf.epsilon, RT_DEFAULT_MAX);
            rtTrace(root, ray, payload);

            // Accumulate radiance
            result += payload.radiance;
            payload.radiance = make_float3(0.f);

            //rtPrintf("depth is: %d and %d\n", payload.depth, cf.maxDepth); 
            // Prepare to shoot next ray
            origin = payload.origin;
            dir = payload.dir;
        } while (!payload.done && payload.depth != cf.maxDepth);
    }
    
    // average out the results 
    result = (result / samples_per_pixel);

    //do
    //{
    //    payload.seed = tea<16>(index * frameID.x, i++);

    //    // Trace a ray (or primary ray)
    //    Ray ray = make_Ray(origin, dir, 0, cf.epsilon, RT_DEFAULT_MAX);
    //    rtTrace(root, ray, payload);

    //    // Accumulate radiance
    //    result += payload.radiance;
    //    payload.radiance = make_float3(0.f);

    //    // Prepare to shoot next ray
    //    origin = payload.origin;
    //    dir = payload.dir;
    //} while (!payload.done && payload.depth != cf.maxDepth);
    //} while (!payload.done && payload.depth != 0);
    
    if (frameID.x == 1) 
        resultBuffer[launchIndex] = result;
    else
    {
        //rtPrintf("frameID is: %d", frameID.x);
        float u = 1.0f / (float)frameID.x;
        float3 oldResult = resultBuffer[launchIndex];
        resultBuffer[launchIndex] = lerp(oldResult, result, u);
    }
}