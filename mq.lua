-- file : application.lua
local module = {}  
m = nil

-- Sends a simple ping to the broker
local function send_ping()  
    m:publish(config.ENDPOINT .. config.ID .. "/heartbeat","id=" .. config.MQID,0,0)
end

-- Sends my id to the broker for registration
local function register_myself()  
    --m:subscribe(config.ENDPOINT .. config.ID,0,function(conn)
--    m:subscribe(config.ENDPOINT .. config.SUB,0,function(conn)
--        print("Successfully subscribed to " .. config.ENDPOINT .. config.SUB)
--    end)
    m:subscribe(config.SUB,0,function(conn)
        print("Successfully subscribed to " .. config.SUB)
    end)
end

local function mqtt_start()  
    m = mqtt.Client(config.MQID, 120, config.MQUSR, config.MQPW)
    -- register message callback beforehand
    m:on("message", function(conn, topic, data) 
      if data ~= nil then
        print(topic .. ": " .. data)
        value = tonumber(data)
        if value ~= nil then
          if (value == 0) then
            rollerblind.up()
          end
          if (value == 100) then
            rollerblind.down()
          end
          if (value > 0 and value < 100) then
            percent_go_to(value,2)
          end
        end      
      end
    end)
    -- Connect to broker
    m:connect(config.MQHOST, config.MQPORT, config.MQTLS, 1, function(con) 
        print("mqtt_start()")
        register_myself()
        tmr.stop(6)
        tmr.alarm(6, 60000, 1, send_ping)
    end) 

end

function module.start()  
  mqtt_start()
end

function module.post_status()
  perc = cur_step*100/tot_steps
  print(config.POST .. ": " .. perc)
  m:publish(config.POST,perc,0,0)
end

return module
