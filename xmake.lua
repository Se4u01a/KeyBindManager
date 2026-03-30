-- include subprojects
includes("lib/commonlibsse-ng")

-- === 1. ДОБАВЛЯЕМ СЮДА: Просим xmake найти и скачать Glaze ===
add_requires("glaze")

local mod_root = "D:/Games/RfaD SE/MO2/mods/KeyBindManager"
local plugins_dir = path.join(mod_root, "SKSE", "Plugins")
local prisma_mod_root = "D:/Games/RfaD SE/MO2/mods/Prisma UI - Next-Gen Web UI Framework"
local prisma_mod_plugins_dir = path.join(prisma_mod_root, "SKSE", "Plugins")
local prisma_runtime_dir = path.join(os.scriptdir(), "vendor", "PrismaUI", "PrismaUI")
local prisma_views_dir = path.join(mod_root, "PrismaUI", "views", "KeyBindManager")
local prisma_ui_dll = path.join(os.scriptdir(), "vendor", "PrismaUI", "PrismaUI.dll")

-- set project constants
set_project("KeyBindManager")
set_version("0.1.0")
set_license("GPL-3.0")
set_languages("c++23")
set_warnings("allextra")

-- add common rules
add_rules("mode.debug", "mode.releasedbg")
add_rules("plugin.vsxmake.autoupdate")

-- define targets
target("KeyBindManager")
    add_rules("commonlibsse-ng.plugin", {
        name = "KeyBindManager",
        author = "libxse",
        description = "SKSE64 plugin template using CommonLibSSE-NG"
    })

    -- === 2. ДОБАВЛЯЕМ СЮДА: Подключаем скачанный Glaze к проекту ===
    add_packages("glaze")

    set_targetdir(plugins_dir)
    add_installfiles("view/(index.html)", { prefixdir = "PrismaUI/views/KeyBindManager" })

    after_build(function(target)
        os.mkdir(plugins_dir)
        if os.isdir(prisma_runtime_dir) then
            os.cp(prisma_runtime_dir, prisma_mod_root)
        else
            print("warning: PrismaUI runtime directory was not found at " .. prisma_runtime_dir)
        end
        os.mkdir(prisma_views_dir)
        if os.isfile(prisma_ui_dll) then
            os.mkdir(prisma_mod_plugins_dir)
            os.cp(prisma_ui_dll, path.join(prisma_mod_plugins_dir, "PrismaUI.dll"))
        else
            print("warning: PrismaUI.dll was not found at " .. prisma_ui_dll)
        end
        os.cp(path.join(os.scriptdir(), "view", "index.html"), path.join(prisma_views_dir, "index.html"))
    end)

    -- add src files
    add_files("src/**.cpp")
    add_headerfiles("src/**.h")
    add_includedirs("src")
    set_pcxxheader("src/pch.h")
