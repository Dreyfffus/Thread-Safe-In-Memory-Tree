-- ConcurrencyTest/premake5.lua
-- Builds a Native Unit Test DLL and links CppUnitTestFramework

local function vs_install_path()
  local pf86 = os.getenv("ProgramFiles(x86)") or "C:\\Program Files (x86)"
  local cmd = '"' .. pf86 ..
    '\\Microsoft Visual Studio\\Installer\\vswhere.exe" ' ..
    '-latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 ' ..
    '-property installationPath'
  local ok, out = pcall(os.outputof, cmd)
  if ok and out and #out > 0 then return (out:gsub("[\r\n]+","")) end
  return os.getenv("VSINSTALLDIR") -- fallback if running from Dev Cmd Prompt
end

local VS = vs_install_path()
assert(VS, "Cannot locate Visual Studio. Install vswhere or set VSINSTALLDIR.")

local UT_INC     = VS .. "\\VC\\Auxiliary\\VS\\UnitTest\\include"
local UT_LIB_X64 = VS .. "\\VC\\Auxiliary\\VS\\UnitTest\\lib\\x64"
local UT_LIB_X86 = VS .. "\\VC\\Auxiliary\\VS\\UnitTest\\lib\\x86"

project "ConcurrencyTest"
  kind "SharedLib"          -- test DLL
  language "C++"
  cppdialect "C++20"
  staticruntime "Off"

  targetdir ("bin/" .. outputdir .. "/%{prj.name}")
  objdir    ("bin-int/" .. outputdir .. "/%{prj.name}")

  files { "ConcurrencyTest.cpp", "pch.h", "pch.cpp" }
  pchheader "pch.h"
  pchsource "pch.cpp"

  includedirs {
    ".",                      -- test folder
    "../ConcurrentTree",      -- library headers
    UT_INC
  }

  links {
    "ConcurrentTree",
    "Microsoft.VisualStudio.TestTools.CppUnitTestFramework"
  }

  filter "architecture:x64"
    libdirs { UT_LIB_X64 }
  filter "architecture:x86"
    libdirs { UT_LIB_X86 }
  filter {}

  filter "configurations:Debug"
    defines { "DEBUG" }
    symbols "On"
  filter "configurations:Release"
    defines { "NDEBUG" }
    optimize "Speed"
  filter {}
