package.cpath = '/usr/local/lua-5.3.3_Linux35_64_lib/lib/lua/5.3/?.so;'

local cjson = require("cjson");


function onAppStart()
	stu_log('==== onAppStart ====')
end

function onAppStop(info)
	stu_log('==== onAppStop ====')
end


function onConnect(client, ...)
	stu_log('==== onConnect ====')
end

function onDisconnect(client)
	stu_log('==== onDisconnect ====')
end


function onMessage(client, message)
	stu_log('onMessage(' .. client.id .. ', ' .. message .. ')')
	
	local data = cjson.decode(message);
	
	if (data.cmd == 'join') then
		return true, '{"raw":"identity","user":{"id":' .. client.id .. ',"name":' .. client.id .. '},"channel":{"id":' .. data.channel.id .. ',"state":2}}'
	end
	
	return true, data.text
end


function stu_log(fmt, ...)
	print('[LUA] ' .. string.format(fmt, table.unpack({...})))
end

stu_log('Loaded script main.lua.')

