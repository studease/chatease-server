package.cpath = '/usr/local/lua-5.3.3_Linux35_64_lib/lib/lua/5.3/?.so;'

local JSON = require("cjson");

local channels = {};


function onAppStart()
	stu_log('==== onAppStart ====')
end

function onAppStop(info)
	stu_log('==== onAppStop ====')
end


function onConnect(client, ...)
	local res = {}, users;
	
	stu_log('==== onConnect ====')
	--[[
	res.raw = 'identity'
	res.user = { id = client.id, name = client.name }
	res.channel = { id = client.channel, role = 0, state = 3 }
	
	users = channels[client.channel];
	if (users == nil) then
		users = {};
		table.insert(channels, client.channel, users)
	end
	
	table.insert(users, client.id, { interval = 1000, active = 0 });
	]]--
	return 101, JSON.encode(res)
end

function onDisconnect(client)
	local users;
	
	stu_log('==== onDisconnect ====')
	--[[
	users = channels[client.channel];
	if (users == nil) then
		return
	end
	]]--
	table.remove(users, client.id);
end


function onMessage(client, message)
	local res = {}, data, users, info;
	
	stu_log('onMessage(' .. client.id .. ', ' .. message .. ')')
	--[[
	data = JSON.decode(message);
	if (data == nil or data.cmd == nil or data.channel == nil or data.channel.id == nil) then
		res.raw = 'error'
		res.error = { code: 400, explain: 'Bad Request' }
		res.channel = data.channel
		
		return res.error.code, JSON.encode(res)
	end
	
	users = channels[data.channel.id]
	if (client.id ~= data.channel.id or users == nil or users[client.id] == nil) then
		res.raw = 'error'
		res.error = { code: 400, explain: 'Bad Request' }
		res.channel = data.channel
		
		return res.error.code, JSON.encode(res)
	end
	
	info = users[client.id]
	-- check interval & permission
	]]--
	return data.type == 'multi' and 200 or 209, data.text
end


function stu_log(fmt, ...)
	print('[LUA] ' .. string.format(fmt, table.unpack({...})))
end

stu_log('Loaded script main.lua.')

