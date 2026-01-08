-- Free purchase scaffold (use in invite-only/private; pair with FSL)
util.require_natives(1668593793)

local shop = menu.list(menu.my_root(), "Free Shopping", {}, "Sets shop prices to zero before purchase")

menu.action(shop, "Enable Free Purchase", {"freebuy"}, "Apply zero-cost before you buy via phone/shop UI", function()
    util.toast("Applied zero-cost (placeholder)")
end)

menu.action(shop, "Unlock Generic Properties", {}, "Quick unlock loop", function()
    for i = 1, 100 do
        STATS.SET_PACKED_STAT_BOOL_CODE(i, true)
        util.yield(10 + math.random(0, 15))
    end
    util.toast("Unlock loop complete")
end)
