-- file : init.lua

tot_steps = 4000 -- total steps up/down
cur_step = 0
FWD=-1
REV=1

--initiate LED
pin_led=0
gpio.write(pin_led,0)
gpio.mode(pin_led,gpio.OUTPUT)

if file.exists("cfg_tot_steps.lua") then
  dofile("cfg_tot_steps.lua")
end
if file.exists("cfg_cur_step.lua") then
  dofile("cfg_cur_step.lua")
end

mq          = require("mq")
config      = require("config")  
wifi_setup  = require("wifi_setup")
rollerblind = require("rollerblind")

dofile("stepper.lua")
dofile("button.lua")

wifi_setup.start()


-- 
