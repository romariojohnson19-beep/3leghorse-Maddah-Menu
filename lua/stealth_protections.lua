-- Stealth protections scaffold for YimMenu Lua
util.require_natives(1668593793)

local protections = menu.list(menu.my_root(), "Stealth Protections", {}, "Lightweight anti-crash/kick helpers")

menu.toggle(protections, "Anti-Desync", {}, "Stops common control losses", function(on)
    if not on then
        util.toast("Anti-Desync off")
        return
    end
    util.toast("Anti-Desync on")
    util.create_tick_handler(function()
        if not NETWORK.NETWORK_IS_IN_SESSION() then return false end
        return true
    end)
end)

menu.toggle_loop(protections, "Block Some Events", {}, "Stub: add hashes to block", function()
    util.yield(500)
end)
