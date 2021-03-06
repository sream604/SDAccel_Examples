/**********
Copyright (c) 2016, Xilinx, Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors
may be used to endorse or promote products derived from this software
without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**********/

/*******************************************************************************
Description: 

    This is a CNN (Convolutional Neural Network) convolutional layer based
    example to showcase the effectiveness of using multiple compute units when
    the base algorithm consists of multiple nested loops with large loop count.    

*******************************************************************************/

#include <iostream>
#include <cstring>
#include <cstdio>
#include <cassert>

//OpenCL utility layer include
#include "xcl.h"
#include "defns.h"

#define WORK_GROUP 4 
#define WORK_ITEM_PER_GROUP 1

#define DATA_SIZE OChan * OSize * OSize

// Software solution
void convGolden(int *weight, int *image, int *out, int i_chan, int o_chan)
{
    // Runs over output filters
    for(int output = 0; output < o_chan; output++){
        // Runs over output pixel in Y-direction
        for(int y = 0; y < OSize; y++){
            // Runs over output pixel in X-direction
            for(int x = 0; x < OSize; x++){
                short acc = 0;
                // Runs over each input channel of input feature map
                for(int input = 0; input < i_chan; input++){
                    // Runs over filter window 
                    for(int i = 0; i < WSize; i++){
                        // Runs over filter windows 
                        for(int j = 0; j < WSize; j++){

                            // Calculate input padding boundaries
            	            int xVal = x*Stride + j-Padding, yVal = y*Stride + i-Padding;

                            // Convolution operation
            	            if(yVal >= 0 && yVal < ISize && xVal >= 0 && xVal < ISize){
		                        acc += (short) image[(input*ISize + yVal)*ISize + xVal] * 
                                       (short) weight[((output*WInChan + input)*WSize + i)*WSize + j];
                            }
                        }
                        // Update each output pixel / output filter
                        out[(output*OSize + y)*OSize + x] = acc;
                    }
                }
            }
        }
    }
}

int main(int argc, char** argv)
{
    int i_chan = IChan;
    int o_chan = OChan;
    
    #ifdef GOOD
        int work_group = WORK_GROUP;
        int work_item_per_group = WORK_ITEM_PER_GROUP;
    #endif
    
    int size = DATA_SIZE;
    
    const char *xcl_emu = getenv("XCL_EMULATION_MODE");
    if(xcl_emu && !strcmp(xcl_emu, "hw_emu")) {
        i_chan = 1;
        o_chan = 1;
        
        #ifdef GOOD
            work_group = 1;
            work_item_per_group = 1;
        #endif
        
        size = o_chan * OSize * OSize;
        
        printf("\nOriginal Dataset is Reduced for Faster Execution of Hardware Emulation Flow\n");
        printf("\t#Input_Channels (IChan)            = %d (Original : 96 )\n", i_chan);
        printf("\t#Weight_Output_Channels (WOutChan) = %d (Original : 256)\n\n", o_chan);
    }
    
    // Allocate Memory in Host (Image, Weights and Output)
    size_t image_size_bytes  = sizeof(int) * i_chan * ISize * ISize;
    size_t weight_size_bytes = sizeof(int) * o_chan * WInChan * WSize * WSize;
    size_t output_size_bytes = sizeof(int) * o_chan * OSize * OSize;

    int *image              = (int *) malloc(image_size_bytes);  assert(image);
    int *weight             = (int *) malloc(weight_size_bytes); assert(weight);
    int *source_hw_results  = (int *) malloc(output_size_bytes); assert(source_hw_results);
    int *source_sw_results  = (int *) malloc(output_size_bytes); assert(source_sw_results);
    
    // Initialize Image, Weights & Output Host Buffers
    for(int i = 0; i < i_chan*ISize*ISize; i++)
        image[i] = i%255;
      
    for(int i = 0; i < o_chan*WInChan*WSize*WSize; i++)
        weight[i] = i%255;

    for(int i = 0; i < o_chan*OSize*OSize; i++)
        source_sw_results[i] = source_hw_results[i] = 0;

//OPENCL HOST CODE AREA START
    //Create Program and Kernels
    xcl_world world = xcl_world_single();
    cl_program program = xcl_import_binary(world, "cnn");
    cl_kernel krnl_cnn_conv = xcl_get_kernel(program, "cnn");

    //Allocate Buffer in Global Memory
    cl_mem buffer_image    = xcl_malloc(world, CL_MEM_READ_ONLY, image_size_bytes);
    cl_mem buffer_weight   = xcl_malloc(world, CL_MEM_READ_ONLY, weight_size_bytes);
    cl_mem buffer_output   = xcl_malloc(world, CL_MEM_WRITE_ONLY, output_size_bytes);

    //Copy input data to device global memory
    xcl_memcpy_to_device(world, buffer_image, image, image_size_bytes);
    xcl_memcpy_to_device(world,buffer_weight,weight, weight_size_bytes);
    
    //Set the Kernel Arguments
    int narg = 0;
    xcl_set_kernel_arg(krnl_cnn_conv, narg++, sizeof(cl_mem), &buffer_image);
    xcl_set_kernel_arg(krnl_cnn_conv, narg++, sizeof(cl_mem), &buffer_weight);
    xcl_set_kernel_arg(krnl_cnn_conv, narg++, sizeof(cl_mem), &buffer_output);
    xcl_set_kernel_arg(krnl_cnn_conv, narg++, sizeof(int), &size);
    xcl_set_kernel_arg(krnl_cnn_conv, narg++, sizeof(int), &i_chan);
    xcl_set_kernel_arg(krnl_cnn_conv, narg++, sizeof(int), &o_chan);
    
    #ifdef GOOD 
        int err = 0; 
        clReleaseCommandQueue(world.command_queue);
        world.command_queue = clCreateCommandQueue(world.context, world.device_id, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE | CL_QUEUE_PROFILING_ENABLE, &err);
        if (err != CL_SUCCESS){
            std::cout << "Error: Failed to create a command queue!" << std::endl;
            std::cout << "Test failed" << std::endl;
            return EXIT_FAILURE;
        }
        //Declare global & local Grids
        cl_uint dimension = 1;
        size_t global_size[dimension];
        size_t local_size[dimension];

        //Set global & local grids
        global_size[0] = work_group;
        local_size[0]  = work_item_per_group; 
        
        //Launch the Kernel
        err = clEnqueueNDRangeKernel(world.command_queue, krnl_cnn_conv, 1, NULL, global_size, local_size, 0, NULL, NULL);
        if(err != CL_SUCCESS){
            printf("Error: failed to execute kernel! %d\n", err);
            printf("Test failed\n");
            exit(EXIT_FAILURE);
        }
        clFinish(world.command_queue);
    #else
        // Launch a single thread to perform the same computation
        // The kernel takes care of running over the whole data set
        xcl_run_kernel3d(world, krnl_cnn_conv, 1, 1, 1);
    #endif

    //Copy Result from Device Global Memory to Host Local Memory
    xcl_memcpy_from_device(world, source_hw_results, buffer_output, output_size_bytes);
    clFinish(world.command_queue);

    //Release Device Memories and Kernels
    clReleaseMemObject(buffer_image);
    clReleaseMemObject(buffer_weight);
    clReleaseMemObject(buffer_output);
    clReleaseKernel(krnl_cnn_conv);
    xcl_release_world(world);
//OPENCL HOST CODE AREA END
    
    convGolden(weight, image, source_sw_results, i_chan, o_chan);

    // Compare the results of the Device to the simulation
    int match = 0;
    for (int i = 0 ; i < size; i++){
        if (source_hw_results[i] != source_sw_results[i]){
            std::cout << "Error: Result mismatch" << std::endl;
            std::cout << "i = " << i << " CPU result = " << source_sw_results[i]
                << " Device result = " << source_hw_results[i] << std::endl;
            match = 1;
            break;
        }
    }
    
    /* Release Memory from Host Memory*/
    free(image);
    free(weight);
    free(source_hw_results);
    free(source_sw_results);

    if (match){
        std::cout << "TEST FAILED." << std::endl; 
        return -1;
    }
    std::cout << "TEST PASSED." << std::endl;
    return 0;
}
