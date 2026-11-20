#include "test_common.hpp"
#include <iostream>
#include <cstdlib>
#include <fstream>
#include <cassert>
#include <string>

int main() {
    configure_test_environment();
    std::cout << "[TEST] Running test_shader_compilation..." << std::endl;

    // 定位 dxc 编译器路径
    const std::string dxc_path = "\"C:\\Program Files (x86)\\Windows Kits\\10\\bin\\10.0.26100.0\\x64\\dxc.exe\"";
    const std::string shader_src = "shaders/kerne_test_compute.hlsl";
    const std::string out_dxil = "build/kerne_test_compute.dxil";

    std::string cmd = dxc_path + " -T cs_6_0 -E cs_main -I shaders " + shader_src + " -Fo " + out_dxil;
    std::cout << "  -> Executing: " << cmd << std::endl;

    int ret = std::system(cmd.c_str());
    assert(ret == 0 && "dxc compilation failed!");

    std::ifstream file(out_dxil, std::ios::binary | std::ios::ate);
    assert(file.is_open() && "DXIL binary output not found!");

    auto file_size = file.tellg();
    std::cout << "  -> Generated DXIL size: " << file_size << " bytes" << std::endl;
    assert(file_size > 1024 && "DXIL binary size too small!");

    std::cout << "[TEST] test_shader_compilation PASSED successfully!" << std::endl;
    return 0;
}
