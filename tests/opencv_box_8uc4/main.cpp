//
// Copyright (c) 2013, University of Erlangen-Nuremberg
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
// ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

#include <iostream>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#ifdef OpenCV
#include "opencv2/gpu/gpu.hpp"
#include "opencv2/imgproc/imgproc_c.h"
#endif

#include "hipacc.hpp"

// variables set by Makefile
//#define SIZE_X 3
//#define SIZE_Y 3
//#define WIDTH 4096
//#define HEIGHT 4096
//#define CPU

#ifdef CPU
#define OFFSET_CV 0.5f
#else
#define OFFSET_CV 0
#endif

using namespace hipacc;


// get time in milliseconds
double time_ms () {
    struct timeval tv;
    gettimeofday (&tv, NULL);

    return ((double)(tv.tv_sec) * 1e+3 + (double)(tv.tv_usec) * 1e-3);
}


// box filter reference
void box_filter(uchar4 *in, uchar4 *out, int size_x, int size_y, int width, int
        height) {
    int anchor_x = size_x >> 1;
    int anchor_y = size_y >> 1;
#ifdef OpenCV
    int upper_x = width-size_x+anchor_x;
    int upper_y = height-size_y+anchor_y;
#else
    int upper_x = width-anchor_x;
    int upper_y = height-anchor_y;
#endif

    for (int y=anchor_y; y<upper_y; ++y) {
        for (int x=anchor_x; x<upper_x; ++x) {
            int4 sum = { 0, 0, 0, 0 };

            // for even filter sizes use -anchor ... +anchor-1
            for (int yf = -anchor_y; yf<=anchor_y; yf++) {
                for (int xf = -anchor_x; xf<=anchor_x; xf++) {
                    sum.x += in[(y + yf)*width + x + xf].x;
                    sum.y += in[(y + yf)*width + x + xf].y;
                    sum.z += in[(y + yf)*width + x + xf].z;
                    sum.w += in[(y + yf)*width + x + xf].w;
                }
            }
            out[y*width + x].x = (unsigned char) (1.0f/(float)(size_x*size_y)*sum.x + OFFSET_CV);
            out[y*width + x].y = (unsigned char) (1.0f/(float)(size_x*size_y)*sum.y + OFFSET_CV);
            out[y*width + x].z = (unsigned char) (1.0f/(float)(size_x*size_y)*sum.z + OFFSET_CV);
            out[y*width + x].w = (unsigned char) (1.0f/(float)(size_x*size_y)*sum.w + OFFSET_CV);
        }
    }
}


// Kernel description in HIPAcc
class BoxFilter : public Kernel<uchar4> {
    private:
        Accessor<uchar4> &Input;
        int size_x, size_y;

    public:
        BoxFilter(IterationSpace<uchar4> &IS, Accessor<uchar4> &Input, int
                size_x, int size_y) :
            Kernel(IS),
            Input(Input),
            size_x(size_x),
            size_y(size_y)
        { addAccessor(&Input); }

        void kernel() {
            int anchor_x = size_x >> 1;
            int anchor_y = size_y >> 1;
            int4 sum = { 0, 0, 0, 0 };

            // for even filter sizes use -anchor ... +anchor-1
            for (int yf = -anchor_y; yf<=anchor_y; yf++) {
                for (int xf = -anchor_x; xf<=anchor_x; xf++) {
                    uchar4 in_p = Input(xf, yf);
                    sum.x += in_p.x;
                    sum.y += in_p.y;
                    sum.z += in_p.z;
                    sum.w += in_p.w;
                }
            }
            uchar4 out_p;
            out_p.x = (unsigned char) (1.0f/(float)(size_x*size_y)*sum.x + OFFSET_CV);
            out_p.y = (unsigned char) (1.0f/(float)(size_x*size_y)*sum.y + OFFSET_CV);
            out_p.z = (unsigned char) (1.0f/(float)(size_x*size_y)*sum.z + OFFSET_CV);
            out_p.w = (unsigned char) (1.0f/(float)(size_x*size_y)*sum.w + OFFSET_CV);
            output() = out_p;
        }
};


int main(int argc, const char **argv) {
    double time0, time1, dt, min_dt;
    const int width = WIDTH;
    const int height = HEIGHT;
    const int size_x = SIZE_X;
    const int size_y = SIZE_Y;
    const int offset_x = size_x >> 1;
    const int offset_y = size_y >> 1;
    float timing = 0.0f;

    // host memory for image of of widthxheight pixels
    uchar4 *host_in = (uchar4 *)malloc(sizeof(uchar4)*width*height);
    uchar4 *host_out = (uchar4 *)malloc(sizeof(uchar4)*width*height);
    uchar4 *reference_in = (uchar4 *)malloc(sizeof(uchar4)*width*height);
    uchar4 *reference_out = (uchar4 *)malloc(sizeof(uchar4)*width*height);

    // input and output image of widthxheight pixels
    Image<uchar4> IN(width, height);
    Image<uchar4> OUT(width, height);
    Accessor<uchar4> AccIn(IN, width-2*offset_x, height-2*offset_y, offset_x, offset_y);

    // initialize data
    for (int y=0; y<height; ++y) {
        for (int x=0; x<width; ++x) {
            host_in[y*width + x].x = (unsigned char)(y*width + x + 1) % 256;
            host_in[y*width + x].y = (unsigned char)(y*width + x + 2) % 256;
            host_in[y*width + x].z = (unsigned char)(y*width + x + 3) % 256;
            host_in[y*width + x].w = (unsigned char)(y*width + x + 4) % 256;
            reference_in[y*width + x] = host_in[y*width + x];
            host_out[y*width + x].x = 0;
            host_out[y*width + x].y = 0;
            host_out[y*width + x].z = 0;
            host_out[y*width + x].w = 0;
            reference_out[y*width + x] = host_out[y*width + x];
        }
    }

    IterationSpace<uchar4> BIS(OUT, width-2*offset_x, height-2*offset_y, offset_x, offset_y);
    BoxFilter BF(BIS, AccIn, size_x, size_y);

    IN = host_in;
    OUT = host_out;

    fprintf(stderr, "Calculating blur filter ...\n");

    BF.execute();
    timing = hipaccGetLastKernelTiming();

    // get results
    host_out = OUT.getData();

    fprintf(stderr, "Hipacc: %.3f ms, %.3f Mpixel/s\n", timing, ((width-2*offset_x)*(height-2*offset_y)/timing)/1000);


#ifdef OpenCV
    // OpenCV uses NPP library for filtering
    // image: 4096x4096
    // kernel size: 3x3
    // offset 3x3 shiftet by 1 -> 1x1
    // output: 4096x4096 - 3x3 -> 4093x4093; start: 1,1; end: 4094,4094
    //
    // image: 4096x4096
    // kernel size: 4x4
    // offset 4x4 shiftet by 1 -> 2x2
    // output: 4096x4096 - 4x4 -> 4092x4092; start: 2,2; end: 4094,4094
#ifdef CPU
    fprintf(stderr, "\nCalculating OpenCV box filter on the CPU ...\n");
#else
    fprintf(stderr, "\nCalculating OpenCV box filter on the GPU ...\n");
#endif


    cv::Mat cv_data_in(height, width, CV_8UC4, host_in);
    cv::Mat cv_data_out(height, width, CV_8UC4, host_out);
    cv::Size ksize(size_x, size_y);
#ifdef CPU
    min_dt = DBL_MAX;
    for (int nt=0; nt<10; nt++) {
        time0 = time_ms();

        cv::blur(cv_data_in, cv_data_out, ksize);

        time1 = time_ms();
        dt = time1 - time0;
        if (dt < min_dt) min_dt = dt;
    }
#else
    cv::gpu::GpuMat gpu_in, gpu_out;
    gpu_in.upload(cv_data_in);

    min_dt = DBL_MAX;
    for (int nt=0; nt<10; nt++) {
        time0 = time_ms();

        cv::gpu::boxFilter(gpu_in, gpu_out, -1, ksize);

        time1 = time_ms();
        dt = time1 - time0;
        if (dt < min_dt) min_dt = dt;
    }

    gpu_out.download(cv_data_out);
#endif

    fprintf(stderr, "OpenCV: %.3f ms, %.3f Mpixel/s\n", min_dt, ((width-size_x)*(height-size_y)/min_dt)/1000);
#endif


    fprintf(stderr, "\nCalculating reference ...\n");
    min_dt = DBL_MAX;
    for (int nt=0; nt<3; nt++) {
        time0 = time_ms();

        // calculate reference
        box_filter(reference_in, reference_out, size_x, size_y, width, height);

        time1 = time_ms();
        dt = time1 - time0;
        if (dt < min_dt) min_dt = dt;
    }
    fprintf(stderr, "Reference: %.3f ms, %.3f Mpixel/s\n", min_dt,
            ((width-2*offset_x)*(height-2*offset_y)/min_dt)/1000);

    fprintf(stderr, "\nComparing results ...\n");
#ifdef OpenCV
    int upper_y = height-size_y+offset_y;
    int upper_x = width-size_x+offset_x;
#else
    int upper_y = height-offset_y;
    int upper_x = width-offset_x;
#endif
    // compare results
    for (int y=offset_y; y<upper_y; y++) {
        for (int x=offset_x; x<upper_x; x++) {
            if (reference_out[y*width + x].x != host_out[y*width +x].x ||
                reference_out[y*width + x].y != host_out[y*width +x].y ||
                reference_out[y*width + x].z != host_out[y*width +x].z ||
                reference_out[y*width + x].w != host_out[y*width +x].w) {
                fprintf(stderr, "Test FAILED, at (%d,%d): %hhu,%hhu,%hhu,%hhu vs. %hhu,%hhu,%hhu,%hhu\n",
                        x, y,
                        reference_out[y*width + x].x,
                        reference_out[y*width + x].y,
                        reference_out[y*width + x].z,
                        reference_out[y*width + x].w,
                        host_out[y*width + x].x,
                        host_out[y*width + x].y,
                        host_out[y*width + x].z,
                        host_out[y*width + x].z);
                exit(EXIT_FAILURE);
            }
        }
    }
    fprintf(stderr, "Test PASSED\n");

    // memory cleanup
    free(host_in);
    //free(host_out);
    free(reference_in);
    free(reference_out);

    return EXIT_SUCCESS;
}
