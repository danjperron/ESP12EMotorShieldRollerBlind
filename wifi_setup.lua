-- file: setup.lua
local module = {}

local function wifi_wait_ip()  
  if wifi.sta.getip()== nil then
    print("IP unavailable, Waiting...")
  else
    tmr.stop(1)
    gpio.write(pin_led,1) --off
    print("\n================== ==================")
    print("ESP8266 mode is: " .. wifi.getmode())
    print("MAC address is: " .. wifi.ap.getmac())
    print("IP is "..wifi.sta.getip())
    print("====================================")
    mq.start()
  end
end

local function wifi_start(list_aps)  
    if list_aps then
        gpio.write(pin_led,0) --on
        for key,value in pairs(list_aps) do
            if config.SSID and config.SSID[key] then
                wifi.setmode(wifi.STATION);
                wifi.sta.config{ssid=key,pwd=config.SSID[key]}
                wifi.sta.connect()
                print("Connecting to " .. key .. " ...")
                --config.SSID = nil  -- can save memory
                tmr.alarm(1, 2500, 1, wifi_wait_ip)
            end
        end
    else
        print("Error getting AP list")
    end
end

function module.start()  
  print("Configuring Wifi ...")
  wifi.setmode(wifi.STATION);
  wifi.sta.getap(wifi_start)
end

return module
