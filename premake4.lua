#!lua
local output = "./build/" .. _ACTION

solution "gou_solution"
   configurations { "Debug", "Release" }

project "gou"
   location (output)
   kind "SharedLib"
   language "C"
   files { "src/**.h", "src/**.cpp" }
   buildoptions { "-Wall" }
   linkoptions { "-lopenal -lEGL -levdev -lpthread -lm -lpng -lasound" }
   defines { "EGL_NO_X11" }
   -- includedirs { "" }

   configuration "Debug"
      flags { "Symbols" }
      defines { "DEBUG" }

   configuration "Release"
      flags { "Optimize" }
      defines { "NDEBUG" }
