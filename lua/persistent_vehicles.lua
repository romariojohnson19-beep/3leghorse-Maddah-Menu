-- persistent_vehicles.lua
util.require_natives("natives-1668593793")

local pv = menu.list(menu.my_root(), "Persistent Vehicles")

menu.action(pv, "Mark Current Vehicle Persistent", {}, "Mark vehicle as bought/owned", function()
    local veh = VEHICLE.GET_VEHICLE_PED_IS_IN(PLAYER.PLAYER_PED_ID(), false)
    if veh ~= 0 then
        DECORATOR.DECOR_SET_BOOL(veh, "pv_spent_money_on_this", true)
        util.toast("Vehicle marked persistent (may save to garage)")
    else
        util.toast("No vehicle found")
    end
end)

menu.action(pv, "Set Vehicle Owned Stats", {}, "Set stat/packed stat to mark ownership", function()
    -- Placeholder: set specific globals/packed stats per vehicle model
    util.toast("Set owned stats (placeholder)")
end)
