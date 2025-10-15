project "ConcurrentTree"
  kind "StaticLib"
  language "C++"

  targetdir ("bin/" .. outputdir .. "/%{prj.name}")
  objdir    ("bin-int/" .. outputdir .. "/%{prj.name}")

  files {
    "concurrent.h",
    "concurrent.cpp"
  }

  includedirs { "." }

  filter "configurations:Debug"
    defines { "DEBUG" }
    symbols "On"

  filter "configurations:Release"
    defines { "NDEBUG" }
    optimize "Speed"