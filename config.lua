-- file : config.lua
local module = {}

module.SSID = {}  
module.SSID["your ssid here"] = "your wifi pw here"

-- example for local MQTT

--module.MQHOST = "ohab.local"
--module.MQPORT = 1883
--module.MQID = node.chipid()
--module.MQUSR = ""
--module.MQPW = ""

-- example for cloud MQTT

module.MQHOST = "m20.cloudmqtt.com"
module.MQPORT = your port here
module.MQID = node.chipid()
module.MQUSR = "you user id here"
module.MQPW = "your pw here"

module.MQTLS = 1 -- 0 = unsecured, 1 = TLS/SSL

module.ENDPOINT = "/house/masterbedroom/rollerblind/"
module.ID = "0"
--module.SUB = "set"
module.SUB = {[module.ENDPOINT .. module.ID .. "/set"]=0,[module.ENDPOINT .. "all"]=0}
module.POST = module.ENDPOINT .. module.ID .. "/status"
return module
 
