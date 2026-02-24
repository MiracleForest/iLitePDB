set_project("iLitePDB")
set_xmakever("2.9.0")
add_rules("mode.debug", "mode.release")
add_rules("plugin.vsxmake.autoupdate")

set_languages("cxx23")
add_repositories("liteldev-repo https://github.com/LiteLDev/xmake-repo.git")

add_requires("llvm-prebuilt")
add_requires("parallel-hashmap 1.35")
add_requires("nlohmann_json")
add_requires("magic_enum")
add_requires("fmt", {configs = {header_only = true}})
add_requires("mimalloc")

target("iLitePDB")
    add_cxflags( "/EHa", "/utf-8", "/W4", "/w44265", "/w44289", "/w44296", "/w45263", "/w44738", "/w45204","/Ob3","/Zo-")
    set_kind("binary")
    set_runtimes("MD")
    add_defines("NOMINMAX", "UNICODE")
    set_symbols("debug")
    add_includedirs("src")
    add_files("src/**.cpp")
    add_headerfiles("src/**.h")
    add_packages(
        "llvm-prebuilt",
        "parallel-hashmap",
        "nlohmann_json",
        "magic_enum",
        "fmt",
        "mimalloc"
    )
    add_links(
        "LLVMDebugInfoPDB",
        "LLVMDebugInfoMSF",
        "LLVMDebugInfoCodeView",
        "LLVMObject",
        "LLVMTextAPI",
        "LLVMMCParser",
        "LLVMIRReader",
        "LLVMAsmParser",
        "LLVMMC",
        "LLVMBitReader",
        "LLVMCore",
        "LLVMRemarks",
        "LLVMBitstreamReader",
        "LLVMBinaryFormat",
        "LLVMTargetParser",
        "LLVMSupport",
        "LLVMDemangle"
    )
