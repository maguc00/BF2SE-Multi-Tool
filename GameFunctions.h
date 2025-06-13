#pragma once
using GetPlayerByID_Fn = Player * (__thiscall*)(PlayerManager*, byte);

struct GameFunctions
{
    GetPlayerByID_Fn GetPlayerByID;
};