workspace "ConcurrentTreeSolution"
  architecture "x64"
  configurations { "Debug", "Release" }
  startproject "ConcurrencyTest"
  system "windows"
  toolset "msc"
  warnings "Extra"
  flags { "MultiProcessorCompile" }
  cppdialect "C++20"
  staticruntime "Off"

  -- VS aligned new
  filter { "action:vs*" }
    buildoptions { "/Zc:alignedNew" }
  filter {}

  outputdir = "%{cfg.buildcfg}/%{cfg.architecture}"

  include "concurrent_tree/premake.lua"
  include "test/premake.lua"