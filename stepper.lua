-- stepper.lua
-- code from: http://www.esp8266.com/viewtopic.php?f=19&t=2326
-- simple stepper driver for controlling a stepper motor with a
-- l293d driver
-- nodemcu pins:  0  5  6  7
stepper_pins = {1,3,2,4} -- (A-)blue, (A+)pink, (B-)yellow, (B+)orange
--stepper_pins = {1,2,3,4}
-- half or full stepping
step_states4 = {
 {1,0,0,1},
 {1,1,0,0},
 {0,1,1,0},
 {0,0,1,1}
}
step_states8 = {
 {1,0,0,0},
 {1,1,0,0},
 {0,1,0,0},
 {0,1,1,0},
 {0,0,1,0},
 {0,0,1,1},
 {0,0,0,1},
 {1,0,0,1},
}
step_states = step_states4 -- choose stepping mode
step_numstates = 4 -- change to match number of rows in step_states
step_delay = 10 -- choose speed
step_state = 0 -- updated by step_take-function
step_direction = 1 -- choose step direction -1, 1
step_stepsleft = 0 -- number of steps to move, will de decremented
step_timerid = 4 -- which timer to use for the steps
status_timerid = 2 -- timer id for posing of status messages
-- setup pins
function pins_enable()
  for i = 1, 4, 1 do
    gpio.mode(stepper_pins[i],gpio.OUTPUT)
  end
end

function pins_disable()
--  for i = 1, 4, 1 do -- no power, all pins
  for i = 2, 4, 1 do -- no power, all pins except one (to keep it in place)
    gpio.mode(stepper_pins[i],gpio.INPUT)
  end
end
-- turn off all pins to let motor rest
function step_stopstate() 
  for i = 1, 4, 1 do
    gpio.write(stepper_pins[i], 0)
  end
end

-- make stepper take one step
function step_take()
  -- jump to the next state in the direction, wrap
  step_state = step_state + step_direction
  cur_step = cur_step + step_direction * FWD
  if step_state > step_numstates then
    step_state = 1;
  elseif step_state < 1 then
    step_state = step_numstates
  end
  -- write the current state to the pins
  pins_enable()
  for i = 1, 4, 1 do
    gpio.write(stepper_pins[i], step_states[step_state][i])
  end
  -- might take another step after step_delay
  step_stepsleft = step_stepsleft-1
  if step_stepsleft > 0 then
--  if cur_step > 0 and cur_step < tot_steps and step_stepsleft > 0 then
    tmr.alarm(step_timerid, step_delay, 0, step_take )
  else
    step_stopstate()
    step_stop()
    pins_disable()
    mq.post_status()
    if file.open("cfg_cur_step.lua", "w+") then
      file.write("cur_step=" .. cur_step .. '\n')
      file.close()
    end
  end
end

-- public method to start moving number of 'int steps' in 'int direction'
function step_move(steps, direction, delay)
  tmr.stop(step_timerid)
  step_stepsleft = steps
  step_direction = direction
  step_delay = delay
  step_take()
end

function step_go_to(step, delay)
  if step >= cur_step then
    steps = step - cur_step
    step_move(steps, FWD, delay)
  end
  if step <= cur_step then
    steps = cur_step - step
    step_move(steps, REV, delay)
  end
end

function percent_go_to(percent, delay)
  if(percent >= 0 and percent <= 100) then
    step_stop()
    tmr.register(status_timerid, 1000, tmr.ALARM_AUTO, function () mq.post_status() end)
    tmr.start(status_timerid)
    step = percent * tot_steps / 100
    step_go_to(step, delay)
  end
end

-- public method to cancel moving
function step_stop()
  tmr.stop(step_timerid)
  tmr.stop(status_timerid)
  step_stepsleft = 0
  step_stopstate()
end
