package.cpath = '/usr/local/lua-5.3.3_Linux35_64_lib/lib/lua/5.3/?.so;'

local JSON = require('cjson')

local channels = {}


function onAppStart()
  log('==== onAppStart ====')
end

function onAppStop(info)
  log('==== onAppStop ====')
end


function onConnect(client, ...)
	local res = {}, users, info
	
	log('==== onConnect ====')
	
	if (client.id == nil or client.channel == nil) then
		res.raw = 'error'
		res.error = { code = 500, explain = 'Internal Server Error' }
		
		return res.error.code, JSON.encode(res)
	end
	
	users = channels[client.channel]
	if (users == nil) then
		users = {}
		channels[client.channel] = users
	end
	
	info = {
		properties = { id = client.id, name = 'u' .. client.id, role = 0 },
		channel = { id = client.channel, state = 0 },
		interval = 1000,
		active = 0
	}
	users[client.id] = info
	
	res.raw = 'ident'
	res.user = info.properties
	res.channel = info.channel
	
	return 101, JSON.encode(res)
end

function onDisconnect(client)
	local users
	
	log('==== onDisconnect ====')
	
	users = channels[client.channel]
	if (users == nil) then
		return
	end
	
	users[client.id] = nil
end


function onMessage(client, message)
	local res = {}, data, users, info
	
	log('==== onMessage ====')
	
	if (client.id == nil or client.channel == nil) then
		res.raw = 'error'
		res.error = { code = 500, explain = 'Internal Server Error' }
		
		return res.error.code, JSON.encode(res)
	end
	
	data = JSON.decode(message)
	if (data == nil or data.cmd == nil) then
		res.raw = 'error'
		res.error = { code = 400, explain = 'Bad Request' }
		
		return res.error.code, JSON.encode(res)
	end
	
	if (data.cmd ~= 'text') then
		res.raw = 'error'
		res.error = { code = 501, explain = 'Not Implemented' }
		
		return res.error.code, JSON.encode(res)
	end
	
	users = channels[data.channel.id]
	if (client.channel ~= data.channel.id or users == nil or users[client.id] == nil) then
		res.raw = 'error'
		res.error = { code = 401, explain = 'Unauthorized' }
		res.channel = data.channel
		
		return res.error.code, JSON.encode(res)
	end
	
	info = users[client.id]
	if (info == nil or info.properties == nil or info.channel == nil) then
		res.raw = 'error'
		res.error = { code = 500, explain = 'Internal Server Error' }
		
		return res.error.code, JSON.encode(res)
	end
	
	res.raw = 'text'
	res.text = data.text
	res.type = data.type
	res.user = info.properties
	
	return data.type == 'multi' and 200 or 209, JSON.encode(res)
end


function log(fmt, ...)
  print('[LUA] ' .. string.format(fmt, table.unpack({ ... })))
end


log('Loaded script main.lua.')

