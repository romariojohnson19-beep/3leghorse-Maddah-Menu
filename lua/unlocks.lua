-- unlocks.lua
util.require_natives("natives-1668593793")

local u = menu.list(menu.my_root(), "Quick Unlocks")

menu.action(u, "Unlock Clothes & Tattoos", {}, "Unlocks common items", function()
    for i = 1, 1000 do
        STATS.SET_PACKED_STAT_BOOL_CODE(i, true)
        util.yield(1)
    end
    util.toast("Unlock loop complete")
end)

menu.action(u, "Unlock Awards & Heists", {}, "Unlocks trophies/heists", function()
    for i = 1001, 1200 do
        STATS.SET_PACKED_STAT_BOOL_CODE(i, true)
        util.yield(1)
    end
    util.toast("Awards & Heists unlocked")
end)
