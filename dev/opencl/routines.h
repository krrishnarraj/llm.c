#include "common.h"

void cl_matmul_forward(GPT2_CL *gcl, float* out,
                    float* inp, float* weight,
                    int B, int T, int C, int OC) {
    // inp is (B,T,C), weight is (OC, C)
    // out will be (B,T,OC)
    cl_int err = 0;

    err = clEnqueueWriteBuffer(gcl->queue, gcl->matmul_inp, CL_TRUE, 0, sizeof(float) * B * T * C, inp, 0, NULL, NULL);
    err |= clEnqueueWriteBuffer(gcl->queue, gcl->matmul_weight, CL_TRUE, 0, sizeof(float) * OC * C, weight, 0, NULL, NULL);
    if (err != CL_SUCCESS)
    {
        printf("error: failed to write to source array!\n");
        exit(1);
    }

    err = clSetKernelArg(gcl->matmul_forward, 3, sizeof(int), &B);
    err |= clSetKernelArg(gcl->matmul_forward, 4, sizeof(int), &T);
    err |= clSetKernelArg(gcl->matmul_forward, 5, sizeof(int), &C);
    err |= clSetKernelArg(gcl->matmul_forward, 6, sizeof(int), &OC);
    if (err != CL_SUCCESS)
    {
        printf("error: failed to set kernel arguments! %d\n", err);
        exit(1);
    }

    gcl->size_global = B * T * OC;
    err = clEnqueueNDRangeKernel(gcl->queue, gcl->matmul_forward, 1, NULL, &gcl->size_global, &gcl->size_local, 0, NULL, NULL);
    if (err != CL_SUCCESS)
    {
        printf("error: failed to execute kernel!\n");
        exit(1);
    }
    clFinish(gcl->queue);

    err = clEnqueueReadBuffer(gcl->queue, gcl->matmul_out, CL_TRUE, 0, sizeof(float) * B * T * OC, out, 0, NULL, NULL );
    if (err != CL_SUCCESS)
    {
        printf("error: failed to read output array! %d\n", err);
        exit(1);
    }
}

void cl_matmul_forward_bias(GPT2_CL *gcl, float* out,
                    float* inp, float* weight, float* bias,
                    int B, int T, int C, int OC) {
    // inp is (B,T,C), weight is (OC, C), bias is (OC)
    // out will be (B,T,OC)
    for (int b = 0; b < B; b++) {
        for (int t = 0; t < T; t++) {
            float* out_bt = out + b * T * OC + t * OC;
            float* inp_bt = inp + b * T * C + t * C;
            for (int o = 0; o < OC; o++) {
                float val = bias[o];
                float* wrow = weight + o*C;
                for (int i = 0; i < C; i++) {
                    val += inp_bt[i] * wrow[i];
                }
                out_bt[o] = val;
            }
        }
    }
}