pico-8 cartridge // http://www.pico-8.com
version 32
__lua__
-- cpu stress test
-- focus on math and loop overhead
-- no sprite rendering

ops = 100
cpu_usage = 0

function _init()
end

function _update()
 -- input to control load
 if (btn(0)) ops -= 50
 if (btn(1)) ops += 50
 if (ops < 0) ops = 0

 -- heavy calculation loop
 local start_t = stat(1)
 for i=1,ops do
  local a = sin(i/100)
  local b = cos(i/100)
  local c = sqrt(a*a + b*b)
  local d = flr(c * 100)
 end
 cpu_usage = stat(1)
end

function _draw()
 cls(1)
 print("cpu stress test", 30, 10, 7)
 print("math ops per frame: "..ops, 20, 30, 7)
 print("left/right to change load", 20, 40, 6)
 
 -- simple progress bar
 rect(10, 60, 10+ops/20, 70, 8)
 
 -- show cpu usage (if fake08 supports stat(1))
 print("cpu: "..flr(cpu_usage*100).."%", 50, 80, 7)
end