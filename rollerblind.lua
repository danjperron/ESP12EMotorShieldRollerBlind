--rollerblind.lua
local module = {}
--local tmr_delay = 2

function module.up()
  print("UP!")
--  if (state == 2 and step_stepsleft == 0) then -- i am down, go up
  if (step_stepsleft == 0) then -- i am down, go up
    print("going up")
    percent_go_to(0,config.step_ms)
    state = 0
  end
end

function module.down()
  print("DOWN!")
--  if (state == 0 and step_stepsleft == 0) then -- i am up, go dowm
  if (step_stepsleft == 0) then -- i am up, go dowm
    print("going down")
    percent_go_to(100,config.step_ms)
    state = 2
  end
end

return module
