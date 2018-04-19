-- file : config.lua
local module = {}

module.SSID = {}  
module.SSID["monEssid"] = "MonPassword"

-- example for local MQTT

--module.MQHOST = "ohab.local"
--module.MQPORT = 1883
--module.MQID = node.chipid()
--module.MQUSR = ""
--module.MQPW = ""

-- example for cloud MQTT

module.MQHOST = "10.11.12.192"
module.MQPORT = 1883
module.MQID = node.chipid()
module.MQUSR = ""
module.MQPW = ""

module.MQTLS = 0 -- 0 = unsecured, 1 = TLS/SSL

module.ENDPOINT = "/house/masterbedroom/rollerblind/"
module.ID = "0"
--missing step_ms
module.step_ms = 1

--module.SUB = "set"
module.SUB = {[module.ENDPOINT .. module.ID .. "/set"]=0,[module.ENDPOINT .. "all"]=0}
module.POST = module.ENDPOINT .. module.ID .. "/status"
return module
 
