#pragma once



// REALLY make sure here that you get an valid pointer otherwise crash
struct c {
    void* vtable; // 0x0
    char pad[0xC];
    float health;   //0x10
    char pad1[0x50];
    float time_to_remove; // 0x64 - usually -1.0f; set it to an high positive number so ragdolls wont despawn
};

struct U_Struct {
    void* vtable;
};

struct WeaponData {
    void* vtable; // 0x0
    char pad0[0x4];
    float* ammo;  // 0x8
};

struct Weapon
{
    void* vtable; //0x0
    char pad0[0x25C];
    float weapon_heat;
};

struct WeaponContainer {
    void* vtable;   // 0x0
    char pad0[0x190];
    WeaponData* weaponData; // 0x194
    char pad1[0x50];
    Weapon* weapon; // 0x1E8
};

struct VehicleData {
    void* vtable; //0x0
    char pad[0x38];
    c* c;               //0x3C Contains Health values, 0 if not used e.g. stationary machine guns
    char pad2[0x78];
    float x_position;   //0xB8
    float y_position;   //0xBC
    float z_position;   //0xC0
    char pad3[0xF4];
    WeaponContainer* weaponContainer;   // 0x1B8
};

struct Vehicle {
    int u_int; //0x0
    VehicleData* vehicleData; //0x4
};

struct CharacterState
{
    void* vtable; //0x0
    char pad0[0x340];
    float stamina; // 0x344
};

struct Character {
    void* vtable; // 0x0
    CharacterState* characterState; // 0x4
};

#pragma pack(push, 1) // Force 1-Byte-Allignment
struct Player
{
    void* vtable;               // 0x00
    char pad[0x54];             // 0x4
    int kit_id;                 // 0x58 - stores current Kit ID from Player; 0 == Special Forces ... 6 == Anti Tank; crashes if out of bounds
    char pad2[0x18];            // 
    int name_length;            // 0x74
    char pad3[0x8];             //
    Vehicle* vehicle;           // 0x80
    U_Struct* unknownStruct;    // 0x84 - contains data about rotation, position and more, but seemingly cant be changed
    char pad4[0x30];            //
    int playerID;               // 0xB8 - playerID; 255 to 0 ?
    char pad5[0x4];             //
    float current_fov_zoom_out; // 0xC0 - cant be changed directly, gets constantly overwritten
    float current_fov_zoom_in;  // 0xC4 - cant be changed directly, gets constantly overwritten
    float zoom_speed;           // 0xC8 - can be changed directly, but gets overwritten; 0.01 slower; 5.0 faster;
    Character* character;       // 0xCC
    char pad6[0x5];             //
    bool is_alive;              // 0xD5
    char pade6[0x2];
    int team;                   // 0xD8 - teamID; 2 or 1
    char pad7[0x30];            //
    int squad_id;               // 0x10C - 0 if not in squad; needs testing; unsure why int not char, game crashes if too big: "Texture not found: menu/hud/texture/ingame/player/icons/minimap/mini_squad_259_small"
    //char pad8[0x4];             //
    bool is_commander;          // 0x110 - if player is the commander; kinda forces an commander but broken.
    bool is_squadleader;        // 0x111 - if player is the squadleader; needs testing
    char pad9[0x52];            //
    int callout_counter;        // 0x164 - used to keep track of Radio calls and messages to prevent spam; 0 to 6
    char pad10[0xD0];           //
    int shot_press_counter;     // 0x238 - used to keep track of how often a player has used the shoot button
    int bullet_in_magazine;     // 0x23C - used to keep track of how many bullets are left in the magazine; cant be changed; vehicles might break the game
    char pad11[0x5];            // for some reason needed, otherwise Padding gets set to 4 Byte allignment and completely fucks it up
};
#pragma pack(pop)

struct PlayerManager
{
    void* vtable;           // 0x00
    char pad_0[0x04];       // 0x04
    void* playerList;       // 0x08
    char pad_1[0x08];       // 0x0C
    int num_players;        // 0x14
    char pad_2[0x48];       // 0x18
    Player* local_player;   // 0x60
};

struct HudColor
{
    void* vtable;       // 0x0
    char pad0[0x174];   // 0x4 -> 0x177
    float crosshair_R;  // 0x178
    float crosshair_G;  // 0x17C
    float crosshair_B;  // 0x180
    float crosshair_A;  // 0x184
};

struct LightManager
{
    void* u_vtable1;
    void* u_vtable2;
    void* u_vtable3;
    int u_param1;       // usually 0x00000000; maybe padding
    int u_param2;       // usually 0x00000001;
    int* unknown_struct; // contains sun position
    float skycolor_R;
    float skycolor_G;
    float skycolor_B;
    float treeSunColor_R;
    float treeSunColor_G;
    float treeSunColor_B;
    float treeSkyColor_R;
    float treeSkyColor_G;
    float treeSkyColor_B;
    float ambientcolor_R;
    float ambientcolor_G;
    float ambientcolor_B;
    float treeAmbientColor_R;
    float treeAmbientColor_G;
    float treeAmbientColor_B;
    float staticSunColor_R;
    float staticSunColor_G;
    float staticSunColor_B;
    float staticSkyColor_R;
    float staticSkyColor_G;
    float staticSkyColor_B;
    float staticSpecularColor_R;
    float staticSpecularColor_G;
    float staticSpecularColor_B;
    float singlePointColor_R;
    float singlePointColor_G;
    float singlePointColor_B;
    float unknown_R;
    float unknown_G;
    float unknown_B;
    char pad_0[0x90];
    float effectSunColor_R;
    float effectSunColor_G;
    float effectSunColor_B;
    float effectShadowColor_R;
    float effectShadowColor_G;
    float effectShadowColor_B;
};

struct Terrain
{
    float sunColor_R;
    float sunColor_G;
    float sunColor_B;
    float GIColor_R;
    float GIColor_G;
    float GIColor_B;
};