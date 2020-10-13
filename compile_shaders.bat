@echo off

set shaders=geometry sky light_directional light_point light_spot post_process

for %%s in (%shaders%) do (
	%VULKAN_SDK%/Bin/glslc.exe Shaders/%%s.vert -o Shaders/%%s.vert.spv
	%VULKAN_SDK%/Bin/glslc.exe Shaders/%%s.frag -o Shaders/%%s.frag.spv
)

%VULKAN_SDK%/Bin/glslc.exe Shaders/shadow.vert -o Shaders/shadow.vert.spv
