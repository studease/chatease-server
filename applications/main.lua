application.onAppStart = 
function onAppStart()
	stu_log('==== onAppStart ====')
end

application.onAppStop = 
function onAppStop(info)
	stu_log('==== onAppStop ====')
end


application.onConnect = 
function onConnect(client, ...)
	stu_log('==== onConnect ====')
end

application.onDisconnect = 
function onDisconnect(client)
	stu_log('==== onDisconnect ====')
end


application.onMessage = 
function onMessage(client, message)
	stu_log('==== onMessage ====')
end


function stu_log(fmt, ...)
	print('[LUA] ' .. string.format(fmt, table.unpack({...})))
end

stu_log('Loaded script main.lua.')

