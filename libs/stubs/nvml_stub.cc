/*
 * Copyright 2025 Monvit
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <nvml.h>
#include <cstdio>
#include <cstring>

extern "C" {

nvmlReturn_t nvmlInit_v2() {
    return NVML_SUCCESS;
}

nvmlReturn_t nvmlShutdown() {
    return NVML_SUCCESS;
}

const char* nvmlErrorString(nvmlReturn_t result) {
    return "Stub: No Error (Simulation)";
}

nvmlReturn_t nvmlDeviceGetHandleByIndex_v2(unsigned int index, nvmlDevice_t* device) {
    if (index > 0) {
        return NVML_ERROR_INVALID_ARGUMENT;
    }
    *device = (nvmlDevice_t)0xDEADBEEF;
    return NVML_SUCCESS;
}

nvmlReturn_t nvmlDeviceGetUUID(nvmlDevice_t device, char* uuid, unsigned int length) {
    snprintf(uuid, length, "GPU-STUB-0000-0000");
    return NVML_SUCCESS;
}

nvmlReturn_t nvmlDeviceGetPowerUsage(nvmlDevice_t device, unsigned int* power) {
    *power = 0;
    return NVML_SUCCESS;
}

nvmlReturn_t nvmlDeviceGetTemperature(nvmlDevice_t device, nvmlTemperatureSensors_t sensorType, unsigned int* temp) {
    *temp = 25;
    return NVML_SUCCESS;
}

nvmlReturn_t nvmlDeviceGetUtilizationRates(nvmlDevice_t device, nvmlUtilization_t* utilization) {
    utilization->gpu = 0;
    utilization->memory = 0;
    return NVML_SUCCESS;
}

nvmlReturn_t nvmlDeviceGetMemoryInfo(nvmlDevice_t device, nvmlMemory_t* memory) {
    memory->total = 8ULL * 1024 * 1024 * 1024;
    memory->free = 4ULL * 1024 * 1024 * 1024;
    memory->used = 4ULL * 1024 * 1024 * 1024;
    return NVML_SUCCESS;
}

nvmlReturn_t nvmlDeviceGetCount_v2(unsigned int* deviceCount) {
    *deviceCount = 1;
    return NVML_SUCCESS;
}

nvmlReturn_t nvmlDeviceGetName(nvmlDevice_t device, char* name, unsigned int length) {
    snprintf(name, length, "NVIDIA GeForce RTX 4090 (Stub)");
    return NVML_SUCCESS;
}

} // extern "C"
