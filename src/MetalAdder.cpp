//
// Created by Гедзь Олесь Денисович on 17.01.2022.
//

#include "MetalAdder.hpp"
#include <iostream>
#include <sstream>
#include <fstream>
const unsigned int array_length = 1 << 5;
const unsigned int buffer_size = array_length * sizeof(float);


std::string readFile(std::filesystem::path p)
{
    auto file = std::ifstream(p);
    assert(file.is_open());
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

void lib_assert(void *lib,   NS::Error* error) {
    if (!lib && error) {
        std::cerr << "Error creating dynamic library from source library "
                  << error->localizedDescription()->cString(NS::ASCIIStringEncoding) << std::endl;
        std::exit(-1);
    }
}

void MetalAdder::init_with_device(MTL::Device* device){
    m_device = device;
    NS::Error* error;
    const auto shaderSrc = readFile("../src/add.metal");
    MTL::Library *library = m_device->newLibrary(NS::String::string(shaderSrc.c_str(), NS::ASCIIStringEncoding), nullptr, &error);
    lib_assert(library, error);

    auto function_name = NS::String::string("add_arrays", NS::ASCIIStringEncoding);
    auto add_function = library->newFunction(function_name);

    if(!add_function){
        std::cerr << "Failed to find the adder function.";
    }

    m_add_function_pso = m_device->newComputePipelineState(add_function, &error);
    m_command_queue = m_device->newCommandQueue();

};







//void MetalAdder::init_with_device(MTL::Device* device){
//    m_device = device;
//    NS::Error* error;
//    NS::String*          pString = NS::String::string( "../src/add.metallib", NS::ASCIIStringEncoding );
//    MTL::CompileOptions *options;
//    options->init();
//    options->setLibraryType(MTL::LibraryTypeDynamic);
//    options->setInstallName(NS::String::string( "../src/add.metallib", NS::ASCIIStringEncoding ));
//
//    std::cout << "Support " << m_device->supportsRenderDynamicLibraries() << std::endl;
//    NS::String* program = NS::String::string( "//  add.metal\n #include <metal_stdlib>\n using namespace metal;\n kernel void add_arrays(device const float* inA,\n device const float* inB, \n device float* result, \n uint index [[thread_position_in_grid]])\n { result[index] = inA[index] + inB[index];}", NS::ASCIIStringEncoding );
//    MTL::Library* smth = m_device->newLibrary(program, options, &error);
//    if (!smth && error) {
//        std::cerr << error->localizedDescription()->cString(NS::ASCIIStringEncoding) << std::endl;
//        std::exit(-1);
//    }
//
////    std::cout << options->installName()->cString(NS::ASCIIStringEncoding) << std::endl;
//    MTL::Library*  default_library = m_device->newLibrary(pString,  &error);//Library(&error);
//    auto  dylib = m_device->newDynamicLibrary(smth, &error);
//    if (dylib)
//     std::cout << dylib->description()->cString(NS::ASCIIStringEncoding) << std::endl;
//    if (!dylib && error) {
//        std::cerr << "Error creating dynamic library from source library " << error->localizedDescription()->cString(NS::ASCIIStringEncoding) << std::endl;
//        std::exit(-1);
//    }
////    MTL::LibraryError::LibraryErrorCompileFailure
////    MTL::
////    auto default_library = m_device->newLibrary()
//    if (!default_library){
//        std::cerr << error->localizedDescription()->cString(NS::ASCIIStringEncoding)  << std::endl;
//        std::exit(-1);
//    }
//
//    auto function_name = NS::String::string("add_arrays", NS::ASCIIStringEncoding);
//    auto add_function = default_library->newFunction(function_name);
//
//    if(!add_function){
//        std::cerr << "Failed to find the adder function.";
//    }
//
//    m_add_function_pso = m_device->newComputePipelineState(add_function, &error);
//    m_command_queue = m_device->newCommandQueue();
//
//};


void MetalAdder::prepare_data(){
    m_buffer_A = m_device->newBuffer(buffer_size, MTL::ResourceStorageModeShared);
    m_buffer_B = m_device->newBuffer(buffer_size, MTL::ResourceStorageModeShared);
    m_buffer_result = m_device->newBuffer(buffer_size, MTL::ResourceStorageModeShared);

    generate_random_float_data(m_buffer_A);
    generate_random_float_data(m_buffer_B);
}


void MetalAdder::generate_random_float_data(MTL::Buffer* buffer){
    float* data_ptr = (float*)buffer->contents();
    for (unsigned long index = 0; index < array_length; index++){
        data_ptr[index] = (float)rand() / (float)(RAND_MAX);
    }
}

void MetalAdder::send_compute_command(){
    MTL::CommandBuffer* command_buffer = m_command_queue->commandBuffer();
//    assert(command_buffer != nullptr);
    MTL::ComputeCommandEncoder* compute_encoder = command_buffer->computeCommandEncoder();
    encode_add_command(compute_encoder);
    compute_encoder->endEncoding();
//    MTL::CommandBufferStatus status = command_buffer->status();
//    std::cout << status << std::endl;
    command_buffer->commit();
    command_buffer->waitUntilCompleted();


    verify_results();
}

void MetalAdder::encode_add_command(MTL::ComputeCommandEncoder* compute_encoder){
    compute_encoder->setComputePipelineState(m_add_function_pso);
    compute_encoder->setBuffer(m_buffer_A, 0, 0);
    compute_encoder->setBuffer(m_buffer_B, 0, 1);
    compute_encoder->setBuffer(m_buffer_result, 0, 2);

    MTL::Size grid_size = MTL::Size(array_length, 1, 1);

    NS::UInteger thread_group_size_ = m_add_function_pso->maxTotalThreadsPerThreadgroup();
    if(thread_group_size_ > array_length){
        thread_group_size_ = array_length;
    }

    MTL::Size thread_group_size = MTL::Size(thread_group_size_, 1, 1);

    compute_encoder->dispatchThreads(grid_size, thread_group_size);
}


void MetalAdder::verify_results(){
    auto a = (float*) m_buffer_A->contents();
    auto b = (float*) m_buffer_B->contents();
    auto result = (float*) m_buffer_result->contents();

    for (unsigned long index = 0; index < array_length; index++){
        if (result[index] != (a[index] + b[index])){
            std::cout << "Comput ERROR: index=" << index << "result=" << result[index] <<  "vs " << a[index] + b[index] << "=a+b\n";
            assert(result[index] == (a[index] + b[index]));
        }
    }
    std::cout << "Compute results as expected\n";
}