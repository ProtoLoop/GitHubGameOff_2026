#include "AnimResources.h"

namespace
{

/// Warrior Run

constexpr const SpriteFrameSequenceDef kWarrior_AnimSeq_Run[] //
    {
        {
            .Name = "Warrior_Run_N",
            .TexturePath = "/assets/warrior_run.png",
            .SrcRect = Rect{ 0, 720, 1728, 144 },
            .Size = Dimension2D{ .w = 144, .h = 144 },
            .ColRect = Rectf{ 0.37f, 0.27f, 0.25f, 0.57f },
            .Pivot{ 0.5f, 0.84f },
        },
        {
            .Name = "Warrior_Run_NE",
            .TexturePath = "/assets/warrior_run.png",
            .SrcRect = Rect{ 0, 576, 1728, 144 },
            .Size = Dimension2D{ .w = 144, .h = 144 },
            .ColRect = Rectf{ 0.37f, 0.27f, 0.25f, 0.57f },
            .Pivot{ 0.5f, 0.84f },
        },
        {
            .Name = "Warrior_Run_E",
            .TexturePath = "/assets/warrior_run.png",
            .SrcRect = Rect{ 0, 432, 1728, 144 },
            .Size = Dimension2D{ .w = 144, .h = 144 },
            .ColRect = Rectf{ 0.37f, 0.27f, 0.25f, 0.57f },
            .Pivot{ 0.5f, 0.84f },
        },
        {
            .Name = "Warrior_Run_SE",
            .TexturePath = "/assets/warrior_run.png",
            .SrcRect = Rect{ 0, 288, 1728, 144 },
            .Size = Dimension2D{ .w = 144, .h = 144 },
            .ColRect = Rectf{ 0.37f, 0.27f, 0.25f, 0.57f },
            .Pivot{ 0.5f, 0.84f },
        },
        {
            .Name = "Warrior_Run_S",
            .TexturePath = "/assets/warrior_run.png",
            .SrcRect = Rect{ 0, 144, 1728, 144 },
            .Size = Dimension2D{ .w = 144, .h = 144 },
            .ColRect = Rectf{ 0.37f, 0.27f, 0.25f, 0.57f },
            .Pivot{ 0.5f, 0.84f },
        },
        {
            .Name = "Warrior_Run_SW",
            .TexturePath = "/assets/warrior_run.png",
            .SrcRect = Rect{ 0, 0, 1728, 144 },
            .Size = Dimension2D{ .w = 144, .h = 144 },
            .ColRect = Rectf{ 0.37f, 0.27f, 0.25f, 0.57f },
            .Pivot{ 0.5f, 0.84f },
        },
        {
            .Name = "Warrior_Run_W",
            .TexturePath = "/assets/warrior_run.png",
            .SrcRect = Rect{ 0, 1008, 1728, 144 },
            .Size = Dimension2D{ .w = 144, .h = 144 },
            .ColRect = Rectf{ 0.37f, 0.27f, 0.25f, 0.57f },
            .Pivot{ 0.5f, 0.84f },
        },
        {
            .Name = "Warrior_Run_NW",
            .TexturePath = "/assets/warrior_run.png",
            .SrcRect = Rect{ 0, 864, 1728, 144 },
            .Size = Dimension2D{ .w = 144, .h = 144 },
            .ColRect = Rectf{ 0.37f, 0.27f, 0.25f, 0.57f },
            .Pivot{ 0.5f, 0.84f },
        },
    };

/// Warrior Idle

constexpr const SpriteFrameSequenceDef kWarrior_AnimSeq_Idle[] //
    {
        {
            .Name = "Warrior_Idle_N",
            .TexturePath = "/assets/warrior_idle.png",
            .SrcRect = Rect{ 0, 720, 1728, 144 },
            .Size = Dimension2D{ .w = 144, .h = 144 },
            .ColRect = Rectf{ 0.37f, 0.27f, 0.25f, 0.57f },
            .Pivot{ 0.5f, 0.84f },
        },
        {
            .Name = "Warrior_Idle_NE",
            .TexturePath = "/assets/warrior_idle.png",
            .SrcRect = Rect{ 0, 576, 1728, 144 },
            .Size = Dimension2D{ .w = 144, .h = 144 },
            .ColRect = Rectf{ 0.37f, 0.27f, 0.25f, 0.57f },
            .Pivot{ 0.5f, 0.84f },
        },
        {
            .Name = "Warrior_Idle_E",
            .TexturePath = "/assets/warrior_idle.png",
            .SrcRect = Rect{ 0, 432, 1728, 144 },
            .Size = Dimension2D{ .w = 144, .h = 144 },
            .ColRect = Rectf{ 0.37f, 0.27f, 0.25f, 0.57f },
            .Pivot{ 0.5f, 0.84f },
        },
        {
            .Name = "Warrior_Idle_SE",
            .TexturePath = "/assets/warrior_idle.png",
            .SrcRect = Rect{ 0, 288, 1728, 144 },
            .Size = Dimension2D{ .w = 144, .h = 144 },
            .ColRect = Rectf{ 0.37f, 0.27f, 0.25f, 0.57f },
            .Pivot{ 0.5f, 0.84f },
        },
        {
            .Name = "Warrior_Idle_S",
            .TexturePath = "/assets/warrior_idle.png",
            .SrcRect = Rect{ 0, 144, 1728, 144 },
            .Size = Dimension2D{ .w = 144, .h = 144 },
            .ColRect = Rectf{ 0.37f, 0.27f, 0.25f, 0.57f },
            .Pivot{ 0.5f, 0.84f },
        },
        {
            .Name = "Warrior_Idle_SW",
            .TexturePath = "/assets/warrior_idle.png",
            .SrcRect = Rect{ 0, 0, 1728, 144 },
            .Size = Dimension2D{ .w = 144, .h = 144 },
            .ColRect = Rectf{ 0.37f, 0.27f, 0.25f, 0.57f },
            .Pivot{ 0.5f, 0.84f },
        },
        {
            .Name = "Warrior_Idle_W",
            .TexturePath = "/assets/warrior_idle.png",
            .SrcRect = Rect{ 0, 1008, 1728, 144 },
            .Size = Dimension2D{ .w = 144, .h = 144 },
            .ColRect = Rectf{ 0.37f, 0.27f, 0.25f, 0.57f },
            .Pivot{ 0.5f, 0.84f },
        },
        {
            .Name = "Warrior_Idle_NW",
            .TexturePath = "/assets/warrior_idle.png",
            .SrcRect = Rect{ 0, 864, 1728, 144 },
            .Size = Dimension2D{ .w = 144, .h = 144 },
            .ColRect = Rectf{ 0.37f, 0.27f, 0.25f, 0.57f },
            .Pivot{ 0.5f, 0.84f },
        },
    };

/// Shadow Hunter Walk

constexpr const SpriteFrameSequenceDef kSH_AnimSeq_Walk[] //
    {
        {
            .Name = "SH_Walk_N",
            .TexturePath = "/assets/shadow_hunter_walk.png",
            .SrcRect = Rect{ 0, 720, 1728, 144 },
            .Size = Dimension2D{ .w = 144, .h = 144 },
            .ColRect = Rectf{ 0, 0, 1, 1 },
            .Pivot{ 0.5f, 1.0f },
        },
        {
            .Name = "SH_Walk_NE",
            .TexturePath = "/assets/shadow_hunter_walk.png",
            .SrcRect = Rect{ 0, 576, 1728, 144 },
            .Size = Dimension2D{ .w = 144, .h = 144 },
            .ColRect = Rectf{ 0, 0, 1, 1 },
            .Pivot{ 0.5f, 1.0f },
        },
        {
            .Name = "SH_Walk_E",
            .TexturePath = "/assets/shadow_hunter_walk.png",
            .SrcRect = Rect{ 0, 432, 1728, 144 },
            .Size = Dimension2D{ .w = 144, .h = 144 },
            .ColRect = Rectf{ 0, 0, 1, 1 },
            .Pivot{ 0.5f, 1.0f },
        },
        {
            .Name = "SH_Walk_SE",
            .TexturePath = "/assets/shadow_hunter_walk.png",
            .SrcRect = Rect{ 0, 288, 1728, 144 },
            .Size = Dimension2D{ .w = 144, .h = 144 },
            .ColRect = Rectf{ 0, 0, 1, 1 },
            .Pivot{ 0.5f, 1.0f },
        },
        {
            .Name = "SH_Walk_S",
            .TexturePath = "/assets/shadow_hunter_walk.png",
            .SrcRect = Rect{ 0, 144, 1728, 144 },
            .Size = Dimension2D{ .w = 144, .h = 144 },
            .ColRect = Rectf{ 0, 0, 1, 1 },
            .Pivot{ 0.5f, 1.0f },
        },
        {
            .Name = "SH_Walk_SW",
            .TexturePath = "/assets/shadow_hunter_walk.png",
            .SrcRect = Rect{ 0, 0, 1728, 144 },
            .Size = Dimension2D{ .w = 144, .h = 144 },
            .ColRect = Rectf{ 0, 0, 1, 1 },
            .Pivot{ 0.5f, 1.0f },
        },
        {
            .Name = "SH_Walk_W",
            .TexturePath = "/assets/shadow_hunter_walk.png",
            .SrcRect = Rect{ 0, 1008, 1728, 144 },
            .Size = Dimension2D{ .w = 144, .h = 144 },
            .ColRect = Rectf{ 0, 0, 1, 1 },
            .Pivot{ 0.5f, 1.0f },
        },
        {
            .Name = "SH_Walk_NW",
            .TexturePath = "/assets/shadow_hunter_walk.png",
            .SrcRect = Rect{ 0, 864, 1728, 144 },
            .Size = Dimension2D{ .w = 144, .h = 144 },
            .ColRect = Rectf{ 0, 0, 1, 1 },
            .Pivot{ 0.5f, 1.0f },
        },
    };

/// Shadow Hunter Hover

constexpr const SpriteFrameSequenceDef kSH_AnimSeq_Hover[] //
    {
        {
            .Name = "SH_Hover_N",
            .TexturePath = "/assets/shadow_hunter_hover.png",
            .SrcRect = Rect{ 0, 720, 1152, 144 },
            .Size = Dimension2D{ .w = 144, .h = 144 },
            .ColRect = Rectf{ 0.37f, 0.17f, 0.25f, 0.53f },
            .Pivot{ 0.5f, 0.84f },
            .Tint = { 0.5f, 0.5f, 1.0f, 0.70f }
        },
        {
            .Name = "SH_Hover_NE",
            .TexturePath = "/assets/shadow_hunter_hover.png",
            .SrcRect = Rect{ 0, 576, 1152, 144 },
            .Size = Dimension2D{ .w = 144, .h = 144 },
            .ColRect = Rectf{ 0.37f, 0.17f, 0.25f, 0.53f },
            .Pivot{ 0.5f, 0.84f },
            .Tint = { 0.5f, 0.5f, 1.0f, 0.70f }
        },
        {
            .Name = "SH_Hover_E",
            .TexturePath = "/assets/shadow_hunter_hover.png",
            .SrcRect = Rect{ 0, 432, 1152, 144 },
            .Size = Dimension2D{ .w = 144, .h = 144 },
            .ColRect = Rectf{ 0.37f, 0.17f, 0.25f, 0.53f },
            .Pivot{ 0.5f, 0.84f },
            .Tint = { 0.5f, 0.5f, 1.0f, 0.70f }
        },
        {
            .Name = "SH_Hover_SE",
            .TexturePath = "/assets/shadow_hunter_hover.png",
            .SrcRect = Rect{ 0, 288, 1152, 144 },
            .Size = Dimension2D{ .w = 144, .h = 144 },
            .ColRect = Rectf{ 0.37f, 0.17f, 0.25f, 0.53f },
            .Pivot{ 0.5f, 0.84f },
            .Tint = { 0.5f, 0.5f, 1.0f, 0.70f }
        },
        {
            .Name = "SH_Hover_S",
            .TexturePath = "/assets/shadow_hunter_hover.png",
            .SrcRect = Rect{ 0, 144, 1152, 144 },
            .Size = Dimension2D{ .w = 144, .h = 144 },
            .ColRect = Rectf{ 0.37f, 0.17f, 0.25f, 0.53f },
            .Pivot{ 0.5f, 0.84f },
            .Tint = { 0.5f, 0.5f, 1.0f, 0.70f }
        },
        {
            .Name = "SH_Hover_SW",
            .TexturePath = "/assets/shadow_hunter_hover.png",
            .SrcRect = Rect{ 0, 0, 1152, 144 },
            .Size = Dimension2D{ .w = 144, .h = 144 },
            .ColRect = Rectf{ 0.37f, 0.17f, 0.25f, 0.53f },
            .Pivot{ 0.5f, 0.84f },
            .Tint = { 0.5f, 0.5f, 1.0f, 0.70f }
        },
        {
            .Name = "SH_Hover_W",
            .TexturePath = "/assets/shadow_hunter_hover.png",
            .SrcRect = Rect{ 0, 1008, 1152, 144 },
            .Size = Dimension2D{ .w = 144, .h = 144 },
            .ColRect = Rectf{ 0.37f, 0.17f, 0.25f, 0.53f },
            .Pivot{ 0.5f, 0.84f },
            .Tint = { 0.5f, 0.5f, 1.0f, 0.70f }
        },
        {
            .Name = "SH_Hover_NW",
            .TexturePath = "/assets/shadow_hunter_hover.png",
            .SrcRect = Rect{ 0, 864, 1152, 144 },
            .Size = Dimension2D{ .w = 144, .h = 144 },
            .ColRect = Rectf{ 0.37f, 0.17f, 0.25f, 0.53f },
            .Pivot{ 0.5f, 0.84f },
            .Tint = { 0.5f, 0.5f, 1.0f, 0.70f }
        },
    };

/// Caretaker Idle

constexpr const SpriteFrameSequenceDef kCT_AnimSeq_Idle[] //
    {
        /// 337 actually points toward zero.
        {
            .Name = "CT_Idle_337",
            .TexturePath = "/assets/anims/Caretaker/Idle/Idle_Body_337.png",
            .Size = Dimension2D{ .w = 180, .h = 180 },
            .ColRect = Rectf{ 0, 0, 1, 1 },
            .Pivot{ 0.5f, 1.0f },
        },
        {
            .Name = "CT_Idle_000",
            .TexturePath = "/assets/anims/Caretaker/Idle/Idle_Body_000.png",
            .Size = Dimension2D{ .w = 180, .h = 180 },
            .ColRect = Rectf{ 0, 0, 1, 1 },
            .Pivot{ 0.5f, 1.0f },
        },
        {
            .Name = "CT_Idle_022",
            .TexturePath = "/assets/anims/Caretaker/Idle/Idle_Body_022.png",
            .Size = Dimension2D{ .w = 180, .h = 180 },
            .ColRect = Rectf{ 0, 0, 1, 1 },
            .Pivot{ 0.5f, 1.0f },
        },
        {
            .Name = "CT_Idle_045",
            .TexturePath = "/assets/anims/Caretaker/Idle/Idle_Body_045.png",
            .Size = Dimension2D{ .w = 180, .h = 180 },
            .ColRect = Rectf{ 0, 0, 1, 1 },
            .Pivot{ 0.5f, 1.0f },
        },
        {
            .Name = "CT_Idle_067",
            .TexturePath = "/assets/anims/Caretaker/Idle/Idle_Body_067.png",
            .Size = Dimension2D{ .w = 180, .h = 180 },
            .ColRect = Rectf{ 0, 0, 1, 1 },
            .Pivot{ 0.5f, 1.0f },
        },
        {
            .Name = "CT_Idle_090",
            .TexturePath = "/assets/anims/Caretaker/Idle/Idle_Body_090.png",
            .Size = Dimension2D{ .w = 180, .h = 180 },
            .ColRect = Rectf{ 0, 0, 1, 1 },
            .Pivot{ 0.5f, 1.0f },
        },
        {
            .Name = "CT_Idle_112",
            .TexturePath = "/assets/anims/Caretaker/Idle/Idle_Body_112.png",
            .Size = Dimension2D{ .w = 180, .h = 180 },
            .ColRect = Rectf{ 0, 0, 1, 1 },
            .Pivot{ 0.5f, 1.0f },
        },
        {
            .Name = "CT_Idle_135",
            .TexturePath = "/assets/anims/Caretaker/Idle/Idle_Body_135.png",
            .Size = Dimension2D{ .w = 180, .h = 180 },
            .ColRect = Rectf{ 0, 0, 1, 1 },
            .Pivot{ 0.5f, 1.0f },
        },
        {
            .Name = "CT_Idle_157",
            .TexturePath = "/assets/anims/Caretaker/Idle/Idle_Body_157.png",
            .Size = Dimension2D{ .w = 180, .h = 180 },
            .ColRect = Rectf{ 0, 0, 1, 1 },
            .Pivot{ 0.5f, 1.0f },
        },
        {
            .Name = "CT_Idle_180",
            .TexturePath = "/assets/anims/Caretaker/Idle/Idle_Body_180.png",
            .Size = Dimension2D{ .w = 180, .h = 180 },
            .ColRect = Rectf{ 0, 0, 1, 1 },
            .Pivot{ 0.5f, 1.0f },
        },
        {
            .Name = "CT_Idle_202",
            .TexturePath = "/assets/anims/Caretaker/Idle/Idle_Body_202.png",
            .Size = Dimension2D{ .w = 180, .h = 180 },
            .ColRect = Rectf{ 0, 0, 1, 1 },
            .Pivot{ 0.5f, 1.0f },
        },
        {
            .Name = "CT_Idle_225",
            .TexturePath = "/assets/anims/Caretaker/Idle/Idle_Body_225.png",
            .Size = Dimension2D{ .w = 180, .h = 180 },
            .ColRect = Rectf{ 0, 0, 1, 1 },
            .Pivot{ 0.5f, 1.0f },
        },
        {
            .Name = "CT_Idle_247",
            .TexturePath = "/assets/anims/Caretaker/Idle/Idle_Body_247.png",
            .Size = Dimension2D{ .w = 180, .h = 180 },
            .ColRect = Rectf{ 0, 0, 1, 1 },
            .Pivot{ 0.5f, 1.0f },
        },
        {
            .Name = "CT_Idle_270",
            .TexturePath = "/assets/anims/Caretaker/Idle/Idle_Body_270.png",
            .Size = Dimension2D{ .w = 180, .h = 180 },
            .ColRect = Rectf{ 0, 0, 1, 1 },
            .Pivot{ 0.5f, 1.0f },
        },
        {
            .Name = "CT_Idle_292",
            .TexturePath = "/assets/anims/Caretaker/Idle/Idle_Body_292.png",
            .Size = Dimension2D{ .w = 180, .h = 180 },
            .ColRect = Rectf{ 0, 0, 1, 1 },
            .Pivot{ 0.5f, 1.0f },
        },
        {
            .Name = "CT_Idle_315",
            .TexturePath = "/assets/anims/Caretaker/Idle/Idle_Body_315.png",
            .Size = Dimension2D{ .w = 180, .h = 180 },
            .ColRect = Rectf{ 0, 0, 1, 1 },
            .Pivot{ 0.5f, 1.0f },
        },
    };

/// Caretaker Walk

constexpr const SpriteFrameSequenceDef kCT_AnimSeq_Walk[] //
    {
        {
            .Name = "CT_Walk_000",
            .TexturePath = "/assets/anims/Caretaker/Walk/Walk_Body_000.png",
            .Size = Dimension2D{ .w = 180, .h = 180 },
            .ColRect = Rectf{ 0, 0, 1, 1 },
            .Pivot{ 0.5f, 1.0f },
        },
        {
            .Name = "CT_Walk_022",
            .TexturePath = "/assets/anims/Caretaker/Walk/Walk_Body_022.png",
            .Size = Dimension2D{ .w = 180, .h = 180 },
            .ColRect = Rectf{ 0, 0, 1, 1 },
            .Pivot{ 0.5f, 1.0f },
        },
        {
            .Name = "CT_Walk_045",
            .TexturePath = "/assets/anims/Caretaker/Walk/Walk_Body_045.png",
            .Size = Dimension2D{ .w = 180, .h = 180 },
            .ColRect = Rectf{ 0, 0, 1, 1 },
            .Pivot{ 0.5f, 1.0f },
        },
        {
            .Name = "CT_Walk_067",
            .TexturePath = "/assets/anims/Caretaker/Walk/Walk_Body_067.png",
            .Size = Dimension2D{ .w = 180, .h = 180 },
            .ColRect = Rectf{ 0, 0, 1, 1 },
            .Pivot{ 0.5f, 1.0f },
        },
        {
            .Name = "CT_Walk_090",
            .TexturePath = "/assets/anims/Caretaker/Walk/Walk_Body_090.png",
            .Size = Dimension2D{ .w = 180, .h = 180 },
            .ColRect = Rectf{ 0, 0, 1, 1 },
            .Pivot{ 0.5f, 1.0f },
        },
        {
            .Name = "CT_Walk_112",
            .TexturePath = "/assets/anims/Caretaker/Walk/Walk_Body_112.png",
            .Size = Dimension2D{ .w = 180, .h = 180 },
            .ColRect = Rectf{ 0, 0, 1, 1 },
            .Pivot{ 0.5f, 1.0f },
        },
        {
            .Name = "CT_Walk_135",
            .TexturePath = "/assets/anims/Caretaker/Walk/Walk_Body_135.png",
            .Size = Dimension2D{ .w = 180, .h = 180 },
            .ColRect = Rectf{ 0, 0, 1, 1 },
            .Pivot{ 0.5f, 1.0f },
        },
        {
            .Name = "CT_Walk_157",
            .TexturePath = "/assets/anims/Caretaker/Walk/Walk_Body_157.png",
            .Size = Dimension2D{ .w = 180, .h = 180 },
            .ColRect = Rectf{ 0, 0, 1, 1 },
            .Pivot{ 0.5f, 1.0f },
        },
        {
            .Name = "CT_Walk_180",
            .TexturePath = "/assets/anims/Caretaker/Walk/Walk_Body_180.png",
            .Size = Dimension2D{ .w = 180, .h = 180 },
            .ColRect = Rectf{ 0, 0, 1, 1 },
            .Pivot{ 0.5f, 1.0f },
        },
        {
            .Name = "CT_Walk_202",
            .TexturePath = "/assets/anims/Caretaker/Walk/Walk_Body_202.png",
            .Size = Dimension2D{ .w = 180, .h = 180 },
            .ColRect = Rectf{ 0, 0, 1, 1 },
            .Pivot{ 0.5f, 1.0f },
        },
        {
            .Name = "CT_Walk_225",
            .TexturePath = "/assets/anims/Caretaker/Walk/Walk_Body_225.png",
            .Size = Dimension2D{ .w = 180, .h = 180 },
            .ColRect = Rectf{ 0, 0, 1, 1 },
            .Pivot{ 0.5f, 1.0f },
        },
        {
            .Name = "CT_Walk_247",
            .TexturePath = "/assets/anims/Caretaker/Walk/Walk_Body_247.png",
            .Size = Dimension2D{ .w = 180, .h = 180 },
            .ColRect = Rectf{ 0, 0, 1, 1 },
            .Pivot{ 0.5f, 1.0f },
        },
        {
            .Name = "CT_Walk_270",
            .TexturePath = "/assets/anims/Caretaker/Walk/Walk_Body_270.png",
            .Size = Dimension2D{ .w = 180, .h = 180 },
            .ColRect = Rectf{ 0, 0, 1, 1 },
            .Pivot{ 0.5f, 1.0f },
        },
        {
            .Name = "CT_Walk_292",
            .TexturePath = "/assets/anims/Caretaker/Walk/Walk_Body_292.png",
            .Size = Dimension2D{ .w = 180, .h = 180 },
            .ColRect = Rectf{ 0, 0, 1, 1 },
            .Pivot{ 0.5f, 1.0f },
        },
        {
            .Name = "CT_Walk_315",
            .TexturePath = "/assets/anims/Caretaker/Walk/Walk_Body_315.png",
            .Size = Dimension2D{ .w = 180, .h = 180 },
            .ColRect = Rectf{ 0, 0, 1, 1 },
            .Pivot{ 0.5f, 1.0f },
        },
        {
            .Name = "CT_Walk_337",
            .TexturePath = "/assets/anims/Caretaker/Walk/Walk_Body_337.png",
            .Size = Dimension2D{ .w = 180, .h = 180 },
            .ColRect = Rectf{ 0, 0, 1, 1 },
            .Pivot{ 0.5f, 1.0f },
        },
    };

/// Caretaker Run

constexpr const SpriteFrameSequenceDef kCT_AnimSeq_Run[] //
    {
        {
            .Name = "CT_Run_000",
            .TexturePath = "/assets/anims/Caretaker/Run/Run_Body_000.png",
            .Size = Dimension2D{ .w = 180, .h = 180 },
            .ColRect = Rectf{ 0, 0, 1, 1 },
            .Pivot{ 0.5f, 1.0f },
        },
        {
            .Name = "CT_Run_022",
            .TexturePath = "/assets/anims/Caretaker/Run/Run_Body_022.png",
            .Size = Dimension2D{ .w = 180, .h = 180 },
            .ColRect = Rectf{ 0, 0, 1, 1 },
            .Pivot{ 0.5f, 1.0f },
        },
        {
            .Name = "CT_Run_045",
            .TexturePath = "/assets/anims/Caretaker/Run/Run_Body_045.png",
            .Size = Dimension2D{ .w = 180, .h = 180 },
            .ColRect = Rectf{ 0, 0, 1, 1 },
            .Pivot{ 0.5f, 1.0f },
        },
        {
            .Name = "CT_Run_067",
            .TexturePath = "/assets/anims/Caretaker/Run/Run_Body_067.png",
            .Size = Dimension2D{ .w = 180, .h = 180 },
            .ColRect = Rectf{ 0, 0, 1, 1 },
            .Pivot{ 0.5f, 1.0f },
        },
        {
            .Name = "CT_Run_090",
            .TexturePath = "/assets/anims/Caretaker/Run/Run_Body_090.png",
            .Size = Dimension2D{ .w = 180, .h = 180 },
            .ColRect = Rectf{ 0, 0, 1, 1 },
            .Pivot{ 0.5f, 1.0f },
        },
        {
            .Name = "CT_Run_112",
            .TexturePath = "/assets/anims/Caretaker/Run/Run_Body_112.png",
            .Size = Dimension2D{ .w = 180, .h = 180 },
            .ColRect = Rectf{ 0, 0, 1, 1 },
            .Pivot{ 0.5f, 1.0f },
        },
        {
            .Name = "CT_Run_135",
            .TexturePath = "/assets/anims/Caretaker/Run/Run_Body_135.png",
            .Size = Dimension2D{ .w = 180, .h = 180 },
            .ColRect = Rectf{ 0, 0, 1, 1 },
            .Pivot{ 0.5f, 1.0f },
        },
        {
            .Name = "CT_Run_157",
            .TexturePath = "/assets/anims/Caretaker/Run/Run_Body_157.png",
            .Size = Dimension2D{ .w = 180, .h = 180 },
            .ColRect = Rectf{ 0, 0, 1, 1 },
            .Pivot{ 0.5f, 1.0f },
        },
        {
            .Name = "CT_Run_180",
            .TexturePath = "/assets/anims/Caretaker/Run/Run_Body_180.png",
            .Size = Dimension2D{ .w = 180, .h = 180 },
            .ColRect = Rectf{ 0, 0, 1, 1 },
            .Pivot{ 0.5f, 1.0f },
        },
        {
            .Name = "CT_Run_202",
            .TexturePath = "/assets/anims/Caretaker/Run/Run_Body_202.png",
            .Size = Dimension2D{ .w = 180, .h = 180 },
            .ColRect = Rectf{ 0, 0, 1, 1 },
            .Pivot{ 0.5f, 1.0f },
        },
        {
            .Name = "CT_Run_225",
            .TexturePath = "/assets/anims/Caretaker/Run/Run_Body_225.png",
            .Size = Dimension2D{ .w = 180, .h = 180 },
            .ColRect = Rectf{ 0, 0, 1, 1 },
            .Pivot{ 0.5f, 1.0f },
        },
        {
            .Name = "CT_Run_247",
            .TexturePath = "/assets/anims/Caretaker/Run/Run_Body_247.png",
            .Size = Dimension2D{ .w = 180, .h = 180 },
            .ColRect = Rectf{ 0, 0, 1, 1 },
            .Pivot{ 0.5f, 1.0f },
        },
        {
            .Name = "CT_Run_270",
            .TexturePath = "/assets/anims/Caretaker/Run/Run_Body_270.png",
            .Size = Dimension2D{ .w = 180, .h = 180 },
            .ColRect = Rectf{ 0, 0, 1, 1 },
            .Pivot{ 0.5f, 1.0f },
        },
        {
            .Name = "CT_Run_292",
            .TexturePath = "/assets/anims/Caretaker/Run/Run_Body_292.png",
            .Size = Dimension2D{ .w = 180, .h = 180 },
            .ColRect = Rectf{ 0, 0, 1, 1 },
            .Pivot{ 0.5f, 1.0f },
        },
        {
            .Name = "CT_Run_315",
            .TexturePath = "/assets/anims/Caretaker/Run/Run_Body_315.png",
            .Size = Dimension2D{ .w = 180, .h = 180 },
            .ColRect = Rectf{ 0, 0, 1, 1 },
            .Pivot{ 0.5f, 1.0f },
        },
        {
            .Name = "CT_Run_337",
            .TexturePath = "/assets/anims/Caretaker/Run/Run_Body_337.png",
            .Size = Dimension2D{ .w = 180, .h = 180 },
            .ColRect = Rectf{ 0, 0, 1, 1 },
            .Pivot{ 0.5f, 1.0f },

        },
    };

constexpr const SpriteAnimDef kAnimDefs[] //
    {
        {
            .Name = AnimResources::kSH_AnimName_Walk,
            .SequenceDefs{ kSH_AnimSeq_Walk },
        },
        {
            .Name = AnimResources::kSH_AnimName_Hover,
            .SequenceDefs{ kSH_AnimSeq_Hover },
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
        {
            .Name = AnimResources::kWarrior_AnimName_Idle,
            .SequenceDefs{ kWarrior_AnimSeq_Idle },
        },
        {
            .Name = AnimResources::kWarrior_AnimName_Run,
            .SequenceDefs{ kWarrior_AnimSeq_Run },
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