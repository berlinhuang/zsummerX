package.path =  "../../depends/proto4z/?.lua;" .. package.path
--require
print("---test-Proto4z-begin--------------------------")
for k,v in pairs(_G.Proto4z) do 
    print(k,v)
end
print("---test-Proto4z-end--------------------------")
require("proto4z")
require("TestProto")

local tb = 
{
    _G.Proto4z,
    _G.summer,
    _G.Proto4zUtil,
}
print("---test-begin--------------------------")
for i,j in pairs(tb) do
    print("________________________")
    for k,v in pairs(j) do
        print(k,v)
    end
end
print("---test-end--------------------------")


print("---test-metatable begin--------------------------")
_G["keytest"] = "sssss"
print(_G["keytest"])
for k,v in pairs(getmetatable(_G)) do
    print(k,v)
end
print("---test-metatable end--------------------------")


local lastTime = os.time()
local echoCount = 0

-- 收到消息
local function whenMessage(sID, pID, content)
    --print("whenMessage. sID=" .. sID .. ", pID=" .. pID )
    --Proto4z.putbin(content)
    local name = Proto4z.getName(pID)
    if name == nil then
        logw("unknown message id recv. pID=" .. pID)
    else
        --echo
        summer.sendContent(sID, pID, content)

        --Proto4z.dump(echo)
        local echo = Proto4z.decode(content, name)
        echoCount = echoCount + 1
        if os.time() - lastTime >=5 then
            print("per second = " .. echoCount/5)
            echoCount = 0
            lastTime = os.time()
        end
    end

end
summer.whenMessage(whenMessage)


-- 连接成功事件
local function whenLinked(sID, remoteIP, remotePort)
    print("session is on connected. sID=" .. sID .. ", remoteIP=" .. remoteIP .. ", remotePort=" .. remotePort)
end
summer.whenLinked(whenLinked)
-- 连接断开事件
local function whenClosed(sID, remoteIP, remotePort)
    print("session is on disconnect. sID=" .. sID .. ", remoteIP=" .. remoteIP .. ", remotePort=" .. remotePort)
end
summer.whenClosed(whenClosed)


--启动网络
summer.start()

--服务器监听
local id = summer.addListen("0.0.0.0",8881)
if id == nil then
    summer.logw("id == nil when addListen")
end
print("new accept id=" .. id)

local startTick = summer.now()
--进入循环
--summer.run()
--如果嵌入其他程序 例如cocos2dx, 可以吧runOnce设置true然后放入update中.
--while summer.runOnce(true) do
while summer.runOnce() and summer.now() - startTick < 10*1000 do
     startTick = summer.now()
end



