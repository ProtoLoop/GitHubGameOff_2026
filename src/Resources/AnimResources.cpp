#include "AnimResources.h"

namespace
{

/// Shadow Hunter Walk

constexpr const SpriteSequenceDef kSH_AnimSeq_Walk[] //
    {
        {
            .Name = "SH_Walk_N",
            .Dim{ .w = 144, .h = 144 }, // NOLINT
            .TexturePath = "/assets/shadow_hunter_walk.png",
            .Rect = Rect{ .x = 0, .y = 720, .w = 1728, .h = 144 } // NOLINT
        },
        {
            .Name = "SH_Walk_NE",
            .Dim{ .w = 144, .h = 144 }, // NOLINT
            .TexturePath = "/assets/shadow_hunter_walk.png",
            .Rect = Rect{ .x = 0, .y = 576, .w = 1728, .h = 144 } // NOLINT
        },
        {
            .Name = "SH_Walk_E",
            .Dim{ .w = 144, .h = 144 }, // NOLINT
            .TexturePath = "/assets/shadow_hunter_walk.png",
            .Rect = Rect{ .x = 0, .y = 432, .w = 1728, .h = 144 } // NOLINT
        },
        {
            .Name = "SH_Walk_SE",
            .Dim{ .w = 144, .h = 144 }, // NOLINT
            .TexturePath = "/assets/shadow_hunter_walk.png",
            .Rect = Rect{ .x = 0, .y = 288, .w = 1728, .h = 144 } // NOLINT
        },
        {
            .Name = "SH_Walk_S",
            .Dim{ .w = 144, .h = 144 }, // NOLINT
            .TexturePath = "/assets/shadow_hunter_walk.png",
            .Rect = Rect{ .x = 0, .y = 144, .w = 1728, .h = 144 } // NOLINT
        },
        {
            .Name = "SH_Walk_SW",
            .Dim{ .w = 144, .h = 144 }, // NOLINT
            .TexturePath = "/assets/shadow_hunter_walk.png",
            .Rect = Rect{ .x = 0, .y = 0, .w = 1728, .h = 144 } // NOLINT
        },
        {
            .Name = "SH_Walk_W",
            .Dim{ .w = 144, .h = 144 }, // NOLINT
            .TexturePath = "/assets/shadow_hunter_walk.png",
            .Rect = Rect{ .x = 0, .y = 1008, .w = 1728, .h = 144 } // NOLINT
        },
        {
            .Name = "SH_Walk_NW",
            .Dim{ .w = 144, .h = 144 }, // NOLINT
            .TexturePath = "/assets/shadow_hunter_walk.png",
            .Rect = Rect{ .x = 0, .y = 864, .w = 1728, .h = 144 } // NOLINT
        },
    };

/// Caretaker Idle

constexpr const SpriteSequenceDef kCT_AnimSeq_Idle[] //
    {
        /// 337 actually points toward zero.
        {
            .Name = "CT_Idle_337",
            .Dim{ .w = 180, .h = 180 }, // NOLINT
            .TexturePath = "/assets/Anims/Caretaker/Idle/Idle_Body_337.png",
        },
        {
            .Name = "CT_Idle_000",
            .Dim{ .w = 180, .h = 180 }, // NOLINT
            .TexturePath = "/assets/Anims/Caretaker/Idle/Idle_Body_000.png",
        },
        {
            .Name = "CT_Idle_022",
            .Dim{ .w = 180, .h = 180 }, // NOLINT
            .TexturePath = "/assets/Anims/Caretaker/Idle/Idle_Body_022.png",
        },
        {
            .Name = "CT_Idle_045",
            .Dim{ .w = 180, .h = 180 }, // NOLINT
            .TexturePath = "/assets/Anims/Caretaker/Idle/Idle_Body_045.png",
        },
        {
            .Name = "CT_Idle_067",
            .Dim{ .w = 180, .h = 180 }, // NOLINT
            .TexturePath = "/assets/Anims/Caretaker/Idle/Idle_Body_067.png",
        },
        {
            .Name = "CT_Idle_090",
            .Dim{ .w = 180, .h = 180 }, // NOLINT
            .TexturePath = "/assets/Anims/Caretaker/Idle/Idle_Body_090.png",
        },
        {
            .Name = "CT_Idle_112",
            .Dim{ .w = 180, .h = 180 }, // NOLINT
            .TexturePath = "/assets/Anims/Caretaker/Idle/Idle_Body_112.png",
        },
        {
            .Name = "CT_Idle_135",
            .Dim{ .w = 180, .h = 180 }, // NOLINT
            .TexturePath = "/assets/Anims/Caretaker/Idle/Idle_Body_135.png",
        },
        {
            .Name = "CT_Idle_157",
            .Dim{ .w = 180, .h = 180 }, // NOLINT
            .TexturePath = "/assets/Anims/Caretaker/Idle/Idle_Body_157.png",
        },
        {
            .Name = "CT_Idle_180",
            .Dim{ .w = 180, .h = 180 }, // NOLINT
            .TexturePath = "/assets/Anims/Caretaker/Idle/Idle_Body_180.png",
        },
        {
            .Name = "CT_Idle_202",
            .Dim{ .w = 180, .h = 180 }, // NOLINT
            .TexturePath = "/assets/Anims/Caretaker/Idle/Idle_Body_202.png",
        },
        {
            .Name = "CT_Idle_225",
            .Dim{ .w = 180, .h = 180 }, // NOLINT
            .TexturePath = "/assets/Anims/Caretaker/Idle/Idle_Body_225.png",
        },
        {
            .Name = "CT_Idle_247",
            .Dim{ .w = 180, .h = 180 }, // NOLINT
            .TexturePath = "/assets/Anims/Caretaker/Idle/Idle_Body_247.png",
        },
        {
            .Name = "CT_Idle_270",
            .Dim{ .w = 180, .h = 180 }, // NOLINT
            .TexturePath = "/assets/Anims/Caretaker/Idle/Idle_Body_270.png",
        },
        {
            .Name = "CT_Idle_292",
            .Dim{ .w = 180, .h = 180 }, // NOLINT
            .TexturePath = "/assets/Anims/Caretaker/Idle/Idle_Body_292.png",
        },
        {
            .Name = "CT_Idle_315",
            .Dim{ .w = 180, .h = 180 }, // NOLINT
            .TexturePath = "/assets/Anims/Caretaker/Idle/Idle_Body_315.png",
        },
    };

/// Caretaker Walk

constexpr const SpriteSequenceDef kCT_AnimSeq_Walk[] //
    {
        {
            .Name = "CT_Walk_000",
            .Dim{ .w = 180, .h = 180 }, // NOLINT
            .TexturePath = "/assets/Anims/Caretaker/Walk/Walk_Body_000.png",
        },
        {
            .Name = "CT_Walk_022",
            .Dim{ .w = 180, .h = 180 }, // NOLINT
            .TexturePath = "/assets/Anims/Caretaker/Walk/Walk_Body_022.png",
        },
        {
            .Name = "CT_Walk_045",
            .Dim{ .w = 180, .h = 180 }, // NOLINT
            .TexturePath = "/assets/Anims/Caretaker/Walk/Walk_Body_045.png",
        },
        {
            .Name = "CT_Walk_067",
            .Dim{ .w = 180, .h = 180 }, // NOLINT
            .TexturePath = "/assets/Anims/Caretaker/Walk/Walk_Body_067.png",
        },
        {
            .Name = "CT_Walk_090",
            .Dim{ .w = 180, .h = 180 }, // NOLINT
            .TexturePath = "/assets/Anims/Caretaker/Walk/Walk_Body_090.png",
        },
        {
            .Name = "CT_Walk_112",
            .Dim{ .w = 180, .h = 180 }, // NOLINT
            .TexturePath = "/assets/Anims/Caretaker/Walk/Walk_Body_112.png",
        },
        {
            .Name = "CT_Walk_135",
            .Dim{ .w = 180, .h = 180 }, // NOLINT
            .TexturePath = "/assets/Anims/Caretaker/Walk/Walk_Body_135.png",
        },
        {
            .Name = "CT_Walk_157",
            .Dim{ .w = 180, .h = 180 }, // NOLINT
            .TexturePath = "/assets/Anims/Caretaker/Walk/Walk_Body_157.png",
        },
        {
            .Name = "CT_Walk_180",
            .Dim{ .w = 180, .h = 180 }, // NOLINT
            .TexturePath = "/assets/Anims/Caretaker/Walk/Walk_Body_180.png",
        },
        {
            .Name = "CT_Walk_202",
            .Dim{ .w = 180, .h = 180 }, // NOLINT
            .TexturePath = "/assets/Anims/Caretaker/Walk/Walk_Body_202.png",
        },
        {
            .Name = "CT_Walk_225",
            .Dim{ .w = 180, .h = 180 }, // NOLINT
            .TexturePath = "/assets/Anims/Caretaker/Walk/Walk_Body_225.png",
        },
        {
            .Name = "CT_Walk_247",
            .Dim{ .w = 180, .h = 180 }, // NOLINT
            .TexturePath = "/assets/Anims/Caretaker/Walk/Walk_Body_247.png",
        },
        {
            .Name = "CT_Walk_270",
            .Dim{ .w = 180, .h = 180 }, // NOLINT
            .TexturePath = "/assets/Anims/Caretaker/Walk/Walk_Body_270.png",
        },
        {
            .Name = "CT_Walk_292",
            .Dim{ .w = 180, .h = 180 }, // NOLINT
            .TexturePath = "/assets/Anims/Caretaker/Walk/Walk_Body_292.png",
        },
        {
            .Name = "CT_Walk_315",
            .Dim{ .w = 180, .h = 180 }, // NOLINT
            .TexturePath = "/assets/Anims/Caretaker/Walk/Walk_Body_315.png",
        },
        {
            .Name = "CT_Walk_337",
            .Dim{ .w = 180, .h = 180 }, // NOLINT
            .TexturePath = "/assets/Anims/Caretaker/Walk/Walk_Body_337.png",
        },
    };

/// Caretaker Run

constexpr const SpriteSequenceDef kCT_AnimSeq_Run[] //
    {
        {
            .Name = "CT_Run_000",
            .Dim{ .w = 180, .h = 180 }, // NOLINT
            .TexturePath = "/assets/Anims/Caretaker/Run/Run_Body_000.png",
        },
        {
            .Name = "CT_Run_022",
            .Dim{ .w = 180, .h = 180 }, // NOLINT
            .TexturePath = "/assets/Anims/Caretaker/Run/Run_Body_022.png",
        },
        {
            .Name = "CT_Run_045",
            .Dim{ .w = 180, .h = 180 }, // NOLINT
            .TexturePath = "/assets/Anims/Caretaker/Run/Run_Body_045.png",
        },
        {
            .Name = "CT_Run_067",
            .Dim{ .w = 180, .h = 180 }, // NOLINT
            .TexturePath = "/assets/Anims/Caretaker/Run/Run_Body_067.png",
        },
        {
            .Name = "CT_Run_090",
            .Dim{ .w = 180, .h = 180 }, // NOLINT
            .TexturePath = "/assets/Anims/Caretaker/Run/Run_Body_090.png",
        },
        {
            .Name = "CT_Run_112",
            .Dim{ .w = 180, .h = 180 }, // NOLINT
            .TexturePath = "/assets/Anims/Caretaker/Run/Run_Body_112.png",
        },
        {
            .Name = "CT_Run_135",
            .Dim{ .w = 180, .h = 180 }, // NOLINT
            .TexturePath = "/assets/Anims/Caretaker/Run/Run_Body_135.png",
        },
        {
            .Name = "CT_Run_157",
            .Dim{ .w = 180, .h = 180 }, // NOLINT
            .TexturePath = "/assets/Anims/Caretaker/Run/Run_Body_157.png",
        },
        {
            .Name = "CT_Run_180",
            .Dim{ .w = 180, .h = 180 }, // NOLINT
            .TexturePath = "/assets/Anims/Caretaker/Run/Run_Body_180.png",
        },
        {
            .Name = "CT_Run_202",
            .Dim{ .w = 180, .h = 180 }, // NOLINT
            .TexturePath = "/assets/Anims/Caretaker/Run/Run_Body_202.png",
        },
        {
            .Name = "CT_Run_225",
            .Dim{ .w = 180, .h = 180 }, // NOLINT
            .TexturePath = "/assets/Anims/Caretaker/Run/Run_Body_225.png",
        },
        {
            .Name = "CT_Run_247",
            .Dim{ .w = 180, .h = 180 }, // NOLINT
            .TexturePath = "/assets/Anims/Caretaker/Run/Run_Body_247.png",
        },
        {
            .Name = "CT_Run_270",
            .Dim{ .w = 180, .h = 180 }, // NOLINT
            .TexturePath = "/assets/Anims/Caretaker/Run/Run_Body_270.png",
        },
        {
            .Name = "CT_Run_292",
            .Dim{ .w = 180, .h = 180 }, // NOLINT
            .TexturePath = "/assets/Anims/Caretaker/Run/Run_Body_292.png",
        },
        {
            .Name = "CT_Run_315",
            .Dim{ .w = 180, .h = 180 }, // NOLINT
            .TexturePath = "/assets/Anims/Caretaker/Run/Run_Body_315.png",
        },
        {
            .Name = "CT_Run_337",
            .Dim{ .w = 180, .h = 180 }, // NOLINT
            .TexturePath = "/assets/Anims/Caretaker/Run/Run_Body_337.png",

        },
    };

constexpr const SpriteAnimDef kAnimDefs[] //
    {
        {
            .Name = AnimResources::kSH_AnimName_Walk,
            .SequenceDefs{ kSH_AnimSeq_Walk },
        },
        {
            .Name = AnimResources::kCT_AnimName_Idle,
            .SequenceDefs{ kCT_AnimSeq_Idle },
        },
        {
            .Name = AnimResources::kCT_AnimName_Walk,
            .SequenceDefs{ kCT_AnimSeq_Walk },
        },
        {
            .Name = AnimResources::kCT_AnimName_Run,
            .SequenceDefs{ kCT_AnimSeq_Run },
        },
    };

} // namespace

namespace AnimResources
{
std::span<const SpriteAnimDef>
GetAnimDefs()
{
    return kAnimDefs;
}
} // namespace AnimResources