@echo off
%VULKAN_SDK%\Bin\glslc.exe Shaders/geometry.vert -o Shaders/geometry.vert.spv
%VULKAN_SDK%\Bin\glslc.exe Shaders/geometry.frag -o Shaders/geometry.frag.spv

%VULKAN_SDK%\Bin\glslc.exe Shaders/screen.vert -o Shaders/screen.vert.spv
%VULKAN_SDK%\Bin\glslc.exe Shaders/screen.frag -o Shaders/screen.frag.spv