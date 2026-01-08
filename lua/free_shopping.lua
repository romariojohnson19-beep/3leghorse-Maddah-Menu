-- free_shopping.lua (2026 compatible)
util.require_natives("natives-1668593793")
local shopping = menu.list(menu.my_root(), "Free Shopping")

menu.action(shopping, "Buy Current Vehicle Free", {}, "Works at Legendary Motorsport etc.", function()
    local veh = VEHICLE.GET_VEHICLE_PED_IS_IN(PLAYER.PLAYER_PED_ID(), false)
    if veh ~= 0 then
        globals.set_int(2359296 + 1 + 24, 0)  -- price override global (tunable)
        util.yield(100)
        PAD.SET_CONTROL_VALUE_NEXT_FRAME(2, 201, 1)  -- Select
    end
end)

menu.action(shopping, "Unlock All Properties", {}, "", function()
    for i = 1, 200 do
        STATS.SET_PACKED_STAT_BOOL_CODE(i, true)
    end
    util.toast("Properties unlocked")
end)
