@echo off
%VULKAN_SDK%\Bin\glslc.exe Shaders\vertex.vert   -o Shaders\vert.spv
%VULKAN_SDK%\Bin\glslc.exe Shaders\fragment.frag -o Shaders\frag.spv
