#pragma once
#include "korl-globalDefines.h"
typedef struct Korl_ImmediateDraw_VertexDescriptor
{
    u16 offsetPosition;
    u16 offsetColor;
} Korl_ImmediateDraw_VertexDescriptor;
typedef struct Korl_ImmediateDraw_Vertex
{
    union 
    {
        struct 
        {
            f32 x, y, z;
        } xyz;
        f32 components[3];
    } position;
    union 
    {
        struct 
        {
            u8 r, g, b;
        } rgb;
        u8 components[3];
    } color;
} Korl_ImmediateDraw_Vertex;
korl_global_const Korl_ImmediateDraw_VertexDescriptor 
    KORL_IMMEDIATEDRAW_DESCRIBEVERTEX_COLOR = 
        { offsetof(Korl_ImmediateDraw_Vertex, position)
        , offsetof(Korl_ImmediateDraw_Vertex, color) };
