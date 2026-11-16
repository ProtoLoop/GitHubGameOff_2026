#define SDL_MAIN_USE_CALLBACKS 1

#include "CellMap.h"
#include "Defer.h"
#include "Game.h"
#include "Input.h"
#include "Level.h"
#include "LevelFetcher.h"
#include "Renderer.h"
#include "Sprite.h"
#include "WebGpu.h"

#include <cstdint>
#include <memory>
#include <numbers>
#include <optional>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_timer.h>
#include <variant>

namespace
{
constexpr const ResourceName kCT_AnimName_Idle = "CT_Idle";
constexpr const ResourceName kCT_AnimName_Run = "CT_Run";
constexpr const ResourceName kCT_AnimName_Walk = "CT_Walk";

constexpr const ResourceName kSH_AnimName_Walk = "SH_Walk";

const SpriteSequenceDef kSH_Walk_SW //
    {
        .Name = "SH_Walk_SW",
        .Dim{ .w = 144, .h = 144 }, // NOLINT
        .TexturePath = "/assets/shadow_hunter_walk.png",
        .Rect = Rect{ .x = 0, .y = 0, .w = 1728, .h = 144 } // NOLINT
    };

const SpriteSequenceDef kSH_Walk_S //
    {
        .Name = "SH_Walk_S",
        .Dim{ .w = 144, .h = 144 }, // NOLINT
        .TexturePath = "/assets/shadow_hunter_walk.png",
        .Rect = Rect{ .x = 0, .y = 144, .w = 1728, .h = 144 } // NOLINT
    };

const SpriteSequenceDef kSH_Walk_SE //
    {
        .Name = "SH_Walk_SE",
        .Dim{ .w = 144, .h = 144 }, // NOLINT
        .TexturePath = "/assets/shadow_hunter_walk.png",
        .Rect = Rect{ .x = 0, .y = 288, .w = 1728, .h = 144 } // NOLINT
    };

const SpriteSequenceDef kSH_Walk_E //
    {
        .Name = "SH_Walk_E",
        .Dim{ .w = 144, .h = 144 }, // NOLINT
        .TexturePath = "/assets/shadow_hunter_walk.png",
        .Rect = Rect{ .x = 0, .y = 432, .w = 1728, .h = 144 } // NOLINT
    };

const SpriteSequenceDef kSH_Walk_NE //
    {
        .Name = "SH_Walk_NE",
        .Dim{ .w = 144, .h = 144 }, // NOLINT
        .TexturePath = "/assets/shadow_hunter_walk.png",
        .Rect = Rect{ .x = 0, .y = 576, .w = 1728, .h = 144 } // NOLINT
    };

const SpriteSequenceDef kSH_Walk_N //
    {
        .Name = "SH_Walk_N",
        .Dim{ .w = 144, .h = 144 }, // NOLINT
        .TexturePath = "/assets/shadow_hunter_walk.png",
        .Rect = Rect{ .x = 0, .y = 720, .w = 1728, .h = 144 } // NOLINT
    };

const SpriteSequenceDef kSH_Walk_NW //
    {
        .Name = "SH_Walk_NW",
        .Dim{ .w = 144, .h = 144 }, // NOLINT
        .TexturePath = "/assets/shadow_hunter_walk.png",
        .Rect = Rect{ .x = 0, .y = 864, .w = 1728, .h = 144 } // NOLINT
    };

const SpriteSequenceDef kSH_Walk_W //
    {
        .Name = "SH_Walk_W",
        .Dim{ .w = 144, .h = 144 }, // NOLINT
        .TexturePath = "/assets/shadow_hunter_walk.png",
        .Rect = Rect{ .x = 0, .y = 1008, .w = 1728, .h = 144 } // NOLINT
    };

/// Caretaker Idle

const SpriteSequenceDef kCT_Idle_000 //
    {
        .Name = "CT_Idle_000",
        .Dim{ .w = 180, .h = 180 }, // NOLINT
        .TexturePath = "/assets/Anims/Caretaker/Idle/Idle_Body_000.png",
        .Rect{ .x = 0, .y = 0, .w = 1080, .h = 720 }, // NOLINT
    };

const SpriteSequenceDef kCT_Idle_022 //
    {
        .Name = "CT_Idle_022",
        .Dim{ .w = 180, .h = 180 }, // NOLINT
        .TexturePath = "/assets/Anims/Caretaker/Idle/Idle_Body_022.png",
        .Rect{ .x = 0, .y = 0, .w = 1080, .h = 720 }, // NOLINT
    };

const SpriteSequenceDef kCT_Idle_045 //
    {
        .Name = "CT_Idle_045",
        .Dim{ .w = 180, .h = 180 }, // NOLINT
        .TexturePath = "/assets/Anims/Caretaker/Idle/Idle_Body_045.png",
        .Rect{ .x = 0, .y = 0, .w = 1080, .h = 720 }, // NOLINT
    };

const SpriteSequenceDef kCT_Idle_067 //
    {
        .Name = "CT_Idle_067",
        .Dim{ .w = 180, .h = 180 }, // NOLINT
        .TexturePath = "/assets/Anims/Caretaker/Idle/Idle_Body_067.png",
        .Rect{ .x = 0, .y = 0, .w = 1080, .h = 720 }, // NOLINT
    };

const SpriteSequenceDef kCT_Idle_090 //
    {
        .Name = "CT_Idle_090",
        .Dim{ .w = 180, .h = 180 }, // NOLINT
        .TexturePath = "/assets/Anims/Caretaker/Idle/Idle_Body_090.png",
        .Rect{ .x = 0, .y = 0, .w = 1080, .h = 720 }, // NOLINT
    };

const SpriteSequenceDef kCT_Idle_112 //
    {
        .Name = "CT_Idle_112",
        .Dim{ .w = 180, .h = 180 }, // NOLINT
        .TexturePath = "/assets/Anims/Caretaker/Idle/Idle_Body_112.png",
        .Rect{ .x = 0, .y = 0, .w = 1080, .h = 720 }, // NOLINT
    };

const SpriteSequenceDef kCT_Idle_135 //
    {
        .Name = "CT_Idle_135",
        .Dim{ .w = 180, .h = 180 }, // NOLINT
        .TexturePath = "/assets/Anims/Caretaker/Idle/Idle_Body_135.png",
        .Rect{ .x = 0, .y = 0, .w = 1080, .h = 720 }, // NOLINT
    };

const SpriteSequenceDef kCT_Idle_157 //
    {
        .Name = "CT_Idle_157",
        .Dim{ .w = 180, .h = 180 }, // NOLINT
        .TexturePath = "/assets/Anims/Caretaker/Idle/Idle_Body_157.png",
        .Rect{ .x = 0, .y = 0, .w = 1080, .h = 720 }, // NOLINT
    };

const SpriteSequenceDef kCT_Idle_180 //
    {
        .Name = "CT_Idle_180",
        .Dim{ .w = 180, .h = 180 }, // NOLINT
        .TexturePath = "/assets/Anims/Caretaker/Idle/Idle_Body_180.png",
        .Rect{ .x = 0, .y = 0, .w = 1080, .h = 720 }, // NOLINT
    };

const SpriteSequenceDef kCT_Idle_202 //
    {
        .Name = "CT_Idle_202",
        .Dim{ .w = 180, .h = 180 }, // NOLINT
        .TexturePath = "/assets/Anims/Caretaker/Idle/Idle_Body_202.png",
        .Rect{ .x = 0, .y = 0, .w = 1080, .h = 720 }, // NOLINT
    };

const SpriteSequenceDef kCT_Idle_225 //
    {
        .Name = "CT_Idle_225",
        .Dim{ .w = 180, .h = 180 }, // NOLINT
        .TexturePath = "/assets/Anims/Caretaker/Idle/Idle_Body_225.png",
        .Rect{ .x = 0, .y = 0, .w = 1080, .h = 720 }, // NOLINT
    };

const SpriteSequenceDef kCT_Idle_247 //
    {
        .Name = "CT_Idle_247",
        .Dim{ .w = 180, .h = 180 }, // NOLINT
        .TexturePath = "/assets/Anims/Caretaker/Idle/Idle_Body_247.png",
        .Rect{ .x = 0, .y = 0, .w = 1080, .h = 720 }, // NOLINT
    };

const SpriteSequenceDef kCT_Idle_270 //
    {
        .Name = "CT_Idle_270",
        .Dim{ .w = 180, .h = 180 }, // NOLINT
        .TexturePath = "/assets/Anims/Caretaker/Idle/Idle_Body_270.png",
        .Rect{ .x = 0, .y = 0, .w = 1080, .h = 720 }, // NOLINT
    };

const SpriteSequenceDef kCT_Idle_292 //
    {
        .Name = "CT_Idle_292",
        .Dim{ .w = 180, .h = 180 }, // NOLINT
        .TexturePath = "/assets/Anims/Caretaker/Idle/Idle_Body_292.png",
        .Rect{ .x = 0, .y = 0, .w = 1080, .h = 720 }, // NOLINT
    };

const SpriteSequenceDef kCT_Idle_315 //
    {
        .Name = "CT_Idle_315",
        .Dim{ .w = 180, .h = 180 }, // NOLINT
        .TexturePath = "/assets/Anims/Caretaker/Idle/Idle_Body_315.png",
        .Rect{ .x = 0, .y = 0, .w = 1080, .h = 720 }, // NOLINT
    };

const SpriteSequenceDef kCT_Idle_337 //
    {
        .Name = "CT_Idle_337",
        .Dim{ .w = 180, .h = 180 }, // NOLINT
        .TexturePath = "/assets/Anims/Caretaker/Idle/Idle_Body_337.png",
        .Rect{ .x = 0, .y = 0, .w = 1080, .h = 720 }, // NOLINT
    };

/// Caretaker Walk

const SpriteSequenceDef kCT_Walk_000 //
    {
        .Name = "CT_Walk_000",
        .Dim{ .w = 180, .h = 180 }, // NOLINT
        .TexturePath = "/assets/Anims/Caretaker/Walk/Walk_Body_000.png",
        .Rect{ .x = 0, .y = 0, .w = 720, .h = 720 }, // NOLINT
    };

const SpriteSequenceDef kCT_Walk_022 //
    {
        .Name = "CT_Walk_022",
        .Dim{ .w = 180, .h = 180 }, // NOLINT
        .TexturePath = "/assets/Anims/Caretaker/Walk/Walk_Body_022.png",
        .Rect{ .x = 0, .y = 0, .w = 720, .h = 720 }, // NOLINT
    };

const SpriteSequenceDef kCT_Walk_045 //
    {
        .Name = "CT_Walk_045",
        .Dim{ .w = 180, .h = 180 }, // NOLINT
        .TexturePath = "/assets/Anims/Caretaker/Walk/Walk_Body_045.png",
        .Rect{ .x = 0, .y = 0, .w = 720, .h = 720 }, // NOLINT
    };

const SpriteSequenceDef kCT_Walk_067 //
    {
        .Name = "CT_Walk_067",
        .Dim{ .w = 180, .h = 180 }, // NOLINT
        .TexturePath = "/assets/Anims/Caretaker/Walk/Walk_Body_067.png",
        .Rect{ .x = 0, .y = 0, .w = 720, .h = 720 }, // NOLINT
    };

const SpriteSequenceDef kCT_Walk_090 //
    {
        .Name = "CT_Walk_090",
        .Dim{ .w = 180, .h = 180 }, // NOLINT
        .TexturePath = "/assets/Anims/Caretaker/Walk/Walk_Body_090.png",
        .Rect{ .x = 0, .y = 0, .w = 720, .h = 720 }, // NOLINT
    };

const SpriteSequenceDef kCT_Walk_112 //
    {
        .Name = "CT_Walk_112",
        .Dim{ .w = 180, .h = 180 }, // NOLINT
        .TexturePath = "/assets/Anims/Caretaker/Walk/Walk_Body_112.png",
        .Rect{ .x = 0, .y = 0, .w = 720, .h = 720 }, // NOLINT
    };

const SpriteSequenceDef kCT_Walk_135 //
    {
        .Name = "CT_Walk_135",
        .Dim{ .w = 180, .h = 180 }, // NOLINT
        .TexturePath = "/assets/Anims/Caretaker/Walk/Walk_Body_135.png",
        .Rect{ .x = 0, .y = 0, .w = 720, .h = 720 }, // NOLINT
    };

const SpriteSequenceDef kCT_Walk_157 //
    {
        .Name = "CT_Walk_157",
        .Dim{ .w = 180, .h = 180 }, // NOLINT
        .TexturePath = "/assets/Anims/Caretaker/Walk/Walk_Body_157.png",
        .Rect{ .x = 0, .y = 0, .w = 720, .h = 720 }, // NOLINT
    };

const SpriteSequenceDef kCT_Walk_180 //
    {
        .Name = "CT_Walk_180",
        .Dim{ .w = 180, .h = 180 }, // NOLINT
        .TexturePath = "/assets/Anims/Caretaker/Walk/Walk_Body_180.png",
        .Rect{ .x = 0, .y = 0, .w = 720, .h = 720 }, // NOLINT
    };

const SpriteSequenceDef kCT_Walk_202 //
    {
        .Name = "CT_Walk_202",
        .Dim{ .w = 180, .h = 180 }, // NOLINT
        .TexturePath = "/assets/Anims/Caretaker/Walk/Walk_Body_202.png",
        .Rect{ .x = 0, .y = 0, .w = 720, .h = 720 }, // NOLINT
    };

const SpriteSequenceDef kCT_Walk_225 //
    {
        .Name = "CT_Walk_225",
        .Dim{ .w = 180, .h = 180 }, // NOLINT
        .TexturePath = "/assets/Anims/Caretaker/Walk/Walk_Body_225.png",
        .Rect{ .x = 0, .y = 0, .w = 720, .h = 720 }, // NOLINT
    };

const SpriteSequenceDef kCT_Walk_247 //
    {
        .Name = "CT_Walk_247",
        .Dim{ .w = 180, .h = 180 }, // NOLINT
        .TexturePath = "/assets/Anims/Caretaker/Walk/Walk_Body_247.png",
        .Rect{ .x = 0, .y = 0, .w = 720, .h = 720 }, // NOLINT
    };

const SpriteSequenceDef kCT_Walk_270 //
    {
        .Name = "CT_Walk_270",
        .Dim{ .w = 180, .h = 180 }, // NOLINT
        .TexturePath = "/assets/Anims/Caretaker/Walk/Walk_Body_270.png",
        .Rect{ .x = 0, .y = 0, .w = 720, .h = 720 }, // NOLINT
    };

const SpriteSequenceDef kCT_Walk_292 //
    {
        .Name = "CT_Walk_292",
        .Dim{ .w = 180, .h = 180 }, // NOLINT
        .TexturePath = "/assets/Anims/Caretaker/Walk/Walk_Body_292.png",
        .Rect{ .x = 0, .y = 0, .w = 720, .h = 720 }, // NOLINT
    };

const SpriteSequenceDef kCT_Walk_315 //
    {
        .Name = "CT_Walk_315",
        .Dim{ .w = 180, .h = 180 }, // NOLINT
        .TexturePath = "/assets/Anims/Caretaker/Walk/Walk_Body_315.png",
        .Rect{ .x = 0, .y = 0, .w = 720, .h = 720 }, // NOLINT
    };

const SpriteSequenceDef kCT_Walk_337 //
    {
        .Name = "CT_Walk_337",
        .Dim{ .w = 180, .h = 180 }, // NOLINT
        .TexturePath = "/assets/Anims/Caretaker/Walk/Walk_Body_337.png",
        .Rect{ .x = 0, .y = 0, .w = 720, .h = 720 }, // NOLINT
    };

/// Caretaker Run

const SpriteSequenceDef kCT_Run_000 //
    {
        .Name = "CT_Run_000",
        .Dim{ .w = 180, .h = 180 }, // NOLINT
        .TexturePath = "/assets/Anims/Caretaker/Run/Run_Body_000.png",
        .Rect{ .x = 0, .y = 0, .w = 1080, .h = 720 }, // NOLINT
    };

const SpriteSequenceDef kCT_Run_022 //
    {
        .Name = "CT_Run_022",
        .Dim{ .w = 180, .h = 180 }, // NOLINT
        .TexturePath = "/assets/Anims/Caretaker/Run/Run_Body_022.png",
        .Rect{ .x = 0, .y = 0, .w = 1080, .h = 720 }, // NOLINT
    };

const SpriteSequenceDef kCT_Run_045 //
    {
        .Name = "CT_Run_045",
        .Dim{ .w = 180, .h = 180 }, // NOLINT
        .TexturePath = "/assets/Anims/Caretaker/Run/Run_Body_045.png",
        .Rect{ .x = 0, .y = 0, .w = 1080, .h = 720 }, // NOLINT
    };

const SpriteSequenceDef kCT_Run_067 //
    {
        .Name = "CT_Run_067",
        .Dim{ .w = 180, .h = 180 }, // NOLINT
        .TexturePath = "/assets/Anims/Caretaker/Run/Run_Body_067.png",
        .Rect{ .x = 0, .y = 0, .w = 1080, .h = 720 }, // NOLINT
    };

const SpriteSequenceDef kCT_Run_090 //
    {
        .Name = "CT_Run_090",
        .Dim{ .w = 180, .h = 180 }, // NOLINT
        .TexturePath = "/assets/Anims/Caretaker/Run/Run_Body_090.png",
        .Rect{ .x = 0, .y = 0, .w = 1080, .h = 720 }, // NOLINT
    };

const SpriteSequenceDef kCT_Run_112 //
    {
        .Name = "CT_Run_112",
        .Dim{ .w = 180, .h = 180 }, // NOLINT
        .TexturePath = "/assets/Anims/Caretaker/Run/Run_Body_112.png",
        .Rect{ .x = 0, .y = 0, .w = 1080, .h = 720 }, // NOLINT
    };

const SpriteSequenceDef kCT_Run_135 //
    {
        .Name = "CT_Run_135",
        .Dim{ .w = 180, .h = 180 }, // NOLINT
        .TexturePath = "/assets/Anims/Caretaker/Run/Run_Body_135.png",
        .Rect{ .x = 0, .y = 0, .w = 1080, .h = 720 }, // NOLINT
    };

const SpriteSequenceDef kCT_Run_157 //
    {
        .Name = "CT_Run_157",
        .Dim{ .w = 180, .h = 180 }, // NOLINT
        .TexturePath = "/assets/Anims/Caretaker/Run/Run_Body_157.png",
        .Rect{ .x = 0, .y = 0, .w = 1080, .h = 720 }, // NOLINT
    };

const SpriteSequenceDef kCT_Run_180 //
    {
        .Name = "CT_Run_180",
        .Dim{ .w = 180, .h = 180 }, // NOLINT
        .TexturePath = "/assets/Anims/Caretaker/Run/Run_Body_180.png",
        .Rect{ .x = 0, .y = 0, .w = 1080, .h = 720 }, // NOLINT
    };

const SpriteSequenceDef kCT_Run_202 //
    {
        .Name = "CT_Run_202",
        .Dim{ .w = 180, .h = 180 }, // NOLINT
        .TexturePath = "/assets/Anims/Caretaker/Run/Run_Body_202.png",
        .Rect{ .x = 0, .y = 0, .w = 1080, .h = 720 }, // NOLINT
    };

const SpriteSequenceDef kCT_Run_225 //
    {
        .Name = "CT_Run_225",
        .Dim{ .w = 180, .h = 180 }, // NOLINT
        .TexturePath = "/assets/Anims/Caretaker/Run/Run_Body_225.png",
        .Rect{ .x = 0, .y = 0, .w = 1080, .h = 720 }, // NOLINT
    };

const SpriteSequenceDef kCT_Run_247 //
    {
        .Name = "CT_Run_247",
        .Dim{ .w = 180, .h = 180 }, // NOLINT
        .TexturePath = "/assets/Anims/Caretaker/Run/Run_Body_247.png",
        .Rect{ .x = 0, .y = 0, .w = 1080, .h = 720 }, // NOLINT
    };

const SpriteSequenceDef kCT_Run_270 //
    {
        .Name = "CT_Run_270",
        .Dim{ .w = 180, .h = 180 }, // NOLINT
        .TexturePath = "/assets/Anims/Caretaker/Run/Run_Body_270.png",
        .Rect{ .x = 0, .y = 0, .w = 1080, .h = 720 }, // NOLINT
    };

const SpriteSequenceDef kCT_Run_292 //
    {
        .Name = "CT_Run_292",
        .Dim{ .w = 180, .h = 180 }, // NOLINT
        .TexturePath = "/assets/Anims/Caretaker/Run/Run_Body_292.png",
        .Rect{ .x = 0, .y = 0, .w = 1080, .h = 720 }, // NOLINT
    };

const SpriteSequenceDef kCT_Run_315 //
    {
        .Name = "CT_Run_315",
        .Dim{ .w = 180, .h = 180 }, // NOLINT
        .TexturePath = "/assets/Anims/Caretaker/Run/Run_Body_315.png",
        .Rect{ .x = 0, .y = 0, .w = 1080, .h = 720 }, // NOLINT
    };

const SpriteSequenceDef kCT_Run_337 //
    {
        .Name = "CT_Run_337",
        .Dim{ .w = 180, .h = 180 }, // NOLINT
        .TexturePath = "/assets/Anims/Caretaker/Run/Run_Body_337.png",
        .Rect{ .x = 0, .y = 0, .w = 1080, .h = 720 }, // NOLINT
    };

const SpriteAnimDef kSH_Walk //
{
    .Name = kSH_AnimName_Walk,
    .SequenceDefs //
    {
        kSH_Walk_N,
        kSH_Walk_NE,
        kSH_Walk_E,
        kSH_Walk_SE,
        kSH_Walk_S,
        kSH_Walk_SW,
        kSH_Walk_W,
        kSH_Walk_NW,
    },
};

const SpriteAnimDef kCtAnimDef2_Idle //
    {
        .Name = kCT_AnimName_Idle,
        .SequenceDefs //
        {
            kCT_Idle_337,
            kCT_Idle_000,
            kCT_Idle_022,
            kCT_Idle_045,
            kCT_Idle_067,
            kCT_Idle_090,
            kCT_Idle_112,
            kCT_Idle_135,
            kCT_Idle_157,
            kCT_Idle_180,
            kCT_Idle_202,
            kCT_Idle_225,
            kCT_Idle_247,
            kCT_Idle_270,
            kCT_Idle_292,
            kCT_Idle_315,
        },
    };

const SpriteAnimDef kCtAnimDef2_Walk //
    {
        .Name = kCT_AnimName_Walk,
        .SequenceDefs //
        {
            kCT_Walk_000,
            kCT_Walk_022,
            kCT_Walk_045,
            kCT_Walk_067,
            kCT_Walk_090,
            kCT_Walk_112,
            kCT_Walk_135,
            kCT_Walk_157,
            kCT_Walk_180,
            kCT_Walk_202,
            kCT_Walk_225,
            kCT_Walk_247,
            kCT_Walk_270,
            kCT_Walk_292,
            kCT_Walk_315,
            kCT_Walk_337,
        },
    };

const SpriteAnimDef kCtAnimDef2_Run //
    {
        .Name = kCT_AnimName_Run,
        .SequenceDefs //
        {
            kCT_Run_000,
            kCT_Run_022,
            kCT_Run_045,
            kCT_Run_067,
            kCT_Run_090,
            kCT_Run_112,
            kCT_Run_135,
            kCT_Run_157,
            kCT_Run_180,
            kCT_Run_202,
            kCT_Run_225,
            kCT_Run_247,
            kCT_Run_270,
            kCT_Run_292,
            kCT_Run_315,
            kCT_Run_337,
        },
    };

const SpriteAnimDef kAnimDefs2[] //
    {
        kCtAnimDef2_Idle,
        kCtAnimDef2_Walk,
        kCtAnimDef2_Run,
    };

/// Number of cells along each axis of the cell map.
constexpr uint32_t kCellMapCellsPerSide = 128;

const CellMapDef kCellMapDef //
    {
        .CellsPerSide = kCellMapCellsPerSide,
        .TileSets //
        {
            "/assets/sample/assets/tiles/dirt_tiles_4x4_256.png",
            "/assets/sample/assets/tiles/grass_tiles_4x4_256.png",
            //"/assets/sample/assets/tiles/coast_sand_05_diff_4k.png",
            //"/assets/sample/assets/tiles/Ground053_4K-PNG_Color.png",
        },
    };

struct App
{
    static constexpr const char* const kWindowTitle = "GraveyardShift";
    static constexpr int kWindowWidth = 800;
    static constexpr int kWindowHeight = 600;

    // Logical width of the display in pixels
    static constexpr uint32_t kLogicalDisplayWidth = 800;

    // Logical height of the display in pixels
    static constexpr uint32_t kLogicalDisplayHeight = 600;

    /// Number of pixels along each axis of a cell.
    static constexpr uint32_t kCellPixelsPerSide = 64;

    /// Number of tiles along each axis in of a tile sheet.
    static constexpr uint32_t kSheetTilesPerSide = 1;

    App() = default;
    App(const App&) = delete;
    App& operator=(const App&) = delete;
    App(App&&) = delete;
    App& operator=(App&&) = delete;

    ~App()
    {
        // Release resources.
        m_State = EndState{};

        if(Window)
        {
            SDL_DestroyWindow(Window);
        }
    }

    enum class Stage
    {
        None,
        InitializingWebGpu,
        WebGpuReady,
        Loading,
        Loaded,
        Running,
        Stopped
    };

    Result<> Update();

    bool IsStopped() const { return m_Stage == Stage::Stopped; }

    Result<> Run();

    SDL_Window* Window = nullptr;
    WebGpu WebGpu;
    Input Input;

    struct LoadingState
    {
        LoadingState(const std::string_view levelPath,
            std::unique_ptr<Renderer> renderer,
            class WebGpu& webGpu)
            : Renderer(std::move(renderer)),
              LevelFetcher(LevelFetcher::Fetch(levelPath, FileFetcher)),
              LevelLoader(
                  LevelLoader::Load(kAnimDefs2, kCellMapDef, FileFetcher, *Renderer, webGpu))
        {
            GG_ASSERT(LevelFetcher, "Failed to initialize level fetcher");
            GG_ASSERT(LevelLoader, "Failed to initialize level loader");
        }

        class FileFetcher FileFetcher;
        std::unique_ptr<Renderer> Renderer;
        Result<LevelFetcher> LevelFetcher;
        Result<LevelLoader> LevelLoader;

        Result<> Update()
        {
            GG_CHECKB(LevelFetcher, "Level fetcher not initialized");
            GG_CHECKB(LevelLoader, "Level loader not initialized");

            GG_CHECK(LevelFetcher->Update(), "Failed to update level fetcher");
            GG_CHECK(LevelLoader->Update(), "Failed to update level loader");

            return Result<>::Ok;
        }

        bool IsPending() const { return LevelFetcher->IsPending() || LevelLoader->IsPending(); }
    };

    struct FailedState
    {
    };

    struct EndState
    {
    };

    std::variant<FailedState, LoadingState, Game, EndState> m_State;

    Stage m_Stage = Stage::None;

    static std::optional<App> m_App;
};

std::optional<App> App::m_App;

[[maybe_unused]] float
TurnsToRadians(uint32_t turns)
{
    return 2.0f
        * std::numbers::pi_v<float>
        * static_cast<float>(turns)
        / static_cast<float>(std::numeric_limits<uint32_t>::max());
}

Result<>
App::Update()
{
    // If we exit due to error then set the stage to Stopped.
    auto stopRunning = GG_DEFER
    {
        m_Stage = Stage::Stopped;
    };

    Input.Update();

    float value{};
    if(Input.IsTriggered(Input::ActionId::Quit, value))
    {
        m_Stage = Stage::Stopped;
    }

    // Call every frame to catch device lost.
    GG_CHECK(WebGpu.Update());

    switch(m_Stage)
    {
        case Stage::None:
            break;
        case Stage::InitializingWebGpu:
            GG_CHECK(WebGpu.Update(), "Failed to update WebGpu");

            if(WebGpu.IsReady())
            {
                m_Stage = Stage::WebGpuReady;
            }
            break;
        case Stage::WebGpuReady:
        {
            const Renderer::Params rendererParams //
                {
                    .LogicalDisplayWidth = kLogicalDisplayWidth,
                    .LogicalDisplayHeight = kLogicalDisplayHeight,
                    .CellPixelsPerSide = kCellPixelsPerSide,
                    .SheetTilesPerSide = kSheetTilesPerSide,
                };

            auto rendererResult = Renderer::Create(rendererParams, WebGpu);
            GG_CHECKB(rendererResult, "Failed to create renderer");

            std::unique_ptr<Renderer> renderer = std::move(*rendererResult);

            m_State.emplace<LoadingState>("/assets/sample/level.json", std::move(renderer), WebGpu);

            m_Stage = Stage::Loading;
        }
        break;
        case Stage::Loading:
        {
            auto& state = std::get<LoadingState>(m_State);

            GG_CHECK(state.Update(), "Failed to update loading state");

            if(!state.IsPending())
            {
                m_Stage = Stage::Loaded;
            }
        }
        break;
        case Stage::Loaded:
        {
            auto& oldState = std::get<LoadingState>(m_State);

            auto levelResult = oldState.LevelLoader->Take();
            GG_CHECKB(levelResult, "Failed to load level");

            // Save the renderer prior to destroying the old state
            std::unique_ptr<Renderer> renderer = std::move(oldState.Renderer);

            m_State.emplace<Game>(Input, std::move(*levelResult), std::move(renderer));

            auto& game = std::get<Game>(m_State);

            GG_CHECKB(game.Begin(), "Failed to begin game");

            m_Stage = Stage::Running;
        }
        break;
        case Stage::Running:
            GG_CHECK(Run(), "Failed to run app");
            break;

        case Stage::Stopped:
            break;
    }

    stopRunning.cancel();

    return Result<>::Ok;
}

Result<>
App::Run()
{
    static uint64_t ms = SDL_GetTicks();

    auto& game = std::get<Game>(m_State);

    const uint64_t curMs = SDL_GetTicks();
    const uint64_t deltaMs = curMs - ms;
    ms = curMs;
    const float deltaSec = static_cast<float>(deltaMs) / 1000.0f;

    GG_CHECKB(game.Tick(deltaSec), "Failed to tick game");

    Input.Reset();

    GG_CHECKB(game.GetRenderer().Render(game.GetLevel()), "Failed to render level");

    GG_CHECKB(WebGpu.Present(), "Failed to present");

    return Result<>::Ok;
}

} // namespace

SDL_AppResult
SDL_AppInit(void** appstate, int, char**)
{
    SDL_SetLogPriorities(SDL_LOG_PRIORITY_TRACE);

    *appstate = nullptr;
    if(!SDL_Init(SDL_INIT_VIDEO))
    {
        GG_ERROR("SDL initialization failed: {}", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    App::m_App.emplace();

    App::m_App->Window = SDL_CreateWindow(App::kWindowTitle,
        App::kWindowWidth,
        App::kWindowHeight,
        SDL_WINDOW_RESIZABLE);

    if(!GG_VERIFYB(App::m_App->Window))
    {
        GG_ERROR("Window creation failed: {}", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if(!App::m_App->WebGpu.Start(App::m_App->Window))
    {
        return SDL_APP_FAILURE;
    }

    App::m_App->m_Stage = App::Stage::InitializingWebGpu;

    *appstate = &App::m_App.value();
    return SDL_APP_CONTINUE;
}

SDL_AppResult
SDL_AppEvent(void* appstate, SDL_Event* event)
{
    App& game = *static_cast<App*>(appstate);
    switch(event->type)
    {
        case SDL_EVENT_QUIT:
            game.Input.TriggerAction(Input::ActionId::Quit, 1);
            break;
        case SDL_EVENT_KEY_DOWN:
            game.Input.TriggerKey(event->key.scancode, Input::Trigger::Pressed);
            break;
        case SDL_EVENT_KEY_UP:
            game.Input.TriggerKey(event->key.scancode, Input::Trigger::Released);
            break;
        default:
            break;
    }

    return SDL_APP_CONTINUE;
}

SDL_AppResult
SDL_AppIterate(void* appstate)
{
    App& app = *static_cast<App*>(appstate);

    if(!app.Update())
    {
        return SDL_APP_FAILURE;
    }

    if(app.IsStopped())
    {
        return SDL_APP_SUCCESS;
    }

    return SDL_APP_CONTINUE;
}

void
SDL_AppQuit(void* /*appstate*/, SDL_AppResult)
{
    // Destroy the renderer and window before shutting down SDL, including on failure.
    App::m_App.reset();
    // SDL calls SDL_Quit after this callback returns.
}
