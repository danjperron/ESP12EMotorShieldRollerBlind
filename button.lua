--file button4.lua

do
  -- use pin 1 as the input pulse width counter
  local pin=5
  local debounce = 150 --ms
  local longpress = 2000 --ms
  local pulse1, pulse2, du, now, trig = 1, 0, 0, tmr.now, gpio.trig
  local prev_int_time, int_time, up_time = 0
  local cal_steps = 100000
  local cal_steps_dn = 0
  local cal_steps_up = 0
  local cal_state = 0 -- 0 = not calibration, 1 = calibrating down, 2 = calibrating up
  state = 0 -- state: 0 = up, 1 = transition, 2 = down
  gpio.mode(pin,gpio.INT)
  
  local function pin4cb(level)
    int_time = now() / 1000
    if ((int_time - prev_int_time) > debounce) then
      if (level == 0) then
        up_time = int_time
      else
        if((int_time - up_time) > longpress) then
          print("calibrating")
          cal_state = 1
          --cur_step = 100000
          step_move(cal_steps,FWD,2)
        else -- short press
          print("short", cal_state)
          if (cal_state == 2) then -- calibrated up (done)
            print("calibration done")
            state = 0 -- up
            cur_step = 0
            tot_steps = cal_steps - step_stepsleft
            print("cal_steps: " .. cal_steps)
            print("step_stepsleft: " .. step_stepsleft)
            print("tot_steps: " .. tot_steps)
            step_stop()
            pins_disable()
            cal_state = 0
            if file.open("cfg_tot_steps.lua", "w+") then
              file.write("tot_steps=" .. tot_steps .. '\n')
              file.close()
            end
          elseif (cal_state == 1) then -- calibrated dn (switch direction)
            print("calibration low point")
            print(cal_steps - step_stepsleft) 
            step_stop()
            step_move(cal_steps,REV,2)
            cal_state = 2
          elseif (cal_state == 0) then
            if (state == 0 and step_stepsleft == 0) then -- i am up, go dowm
              rollerblind.down()
--              state = 2
            elseif (state == 1) then -- i am moving, do nothing
              -- do nothing
            elseif (state == 2 and step_stepsleft == 0) then -- i am down, go up
              rollerblind.up()
--              state = 0
            end
          end
        end
      end
      --print (level)
      prev_int_time = int_time
    end
  end
  gpio.trig(pin, "both", pin4cb)
end
