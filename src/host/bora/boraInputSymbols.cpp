#include "boraInputSymbols.h"
#include <iostream>
#include <cstring>

int32_t BoraInputSymbols::host_cin(wasm_exec_env_t exec_env, int32_t app_ptr, int32_t len) {
    wasm_module_inst_t module_inst = wasm_runtime_get_module_inst(exec_env);
    void *native_buff = wasm_runtime_addr_app_to_native(module_inst, (uint64_t)app_ptr);
    if (!native_buff || len <= 0) return -1;

    std::cout << "Enter an input: ";
    std::string input;
    std::getline(std::cin, input);

    if ((int) input.length() >= len) input.resize(len - 1);
    memcpy(native_buff, input.c_str(), input.length());
    static_cast<char*>(native_buff)[input.length()] = '\0';

    return input.length();
}