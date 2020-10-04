@echo off
%VULKAN_SDK%\Bin\glslc.exe Shaders/geometry.vert -o Shaders/geometry.vert.spv
%VULKAN_SDK%\Bin\glslc.exe Shaders/geometry.frag -o Shaders/geometry.frag.spv

%VULKAN_SDK%\Bin\glslc.exe Shaders/light_directional.vert -o Shaders/light_directional.vert.spv
%VULKAN_SDK%\Bin\glslc.exe Shaders/light_directional.frag -o Shaders/light_directional.frag.spv

%VULKAN_SDK%\Bin\glslc.exe Shaders/light_point.vert -o Shaders/light_point.vert.spv
%VULKAN_SDK%\Bin\glslc.exe Shaders/light_point.frag -o Shaders/light_point.frag.spv
