#chuck

* first, Chuck is my son's name.

* second, Chuck is a high perference,asynchronous and easily use C/Lua network library under Linux/Freebsd.

#build

download and make [lua 5.3](http://www.lua.org/)

static library for c:

	make libchuck

dynamic library for lua:

	make chuck.so
	make packet.so


#examples

##echo.lua

	local chuck = require("chuck")
	local socket = chuck.socket

	local event_loop = chuck.event_loop.New()

	local server = socket.stream.ip4.listen(event_loop,"127.0.0.1",8010,function (fd)
		local conn = socket.stream.New(fd,4096)
		if conn then
			conn:Bind(event_loop,function (data)
				if data then 
					print(data:Content())
					local response = data:Clone()
					response:AppendStr("hello world\r\n")
					conn:Send(response)
				else
					print("client disconnected") 
					conn:Close() 
				end
			end)
		end
	end)

	if server then
		event_loop:Run()
	end

##broadcast_svr.lua

	local chuck = require("chuck")
	local socket = chuck.socket
	local packet = require("packet")

	local event_loop = chuck.event_loop.New()

	local clients = {}
	local client_count = 0
	local packet_count = 0

	local server = socket.stream.ip4.listen(event_loop,"127.0.0.1",8010,function (fd)
		local conn = socket.stream.New(fd,65536,packet.Decoder())
		if conn then
			clients[fd] = conn
			client_count = client_count + 1
			conn:Bind(event_loop,function (data)
				if data then 
					for k,v in pairs(clients) do
						packet_count = packet_count + 1
						v:Send(data)
					end
				else
					client_count = client_count - 1
					print("client disconnected") 
					conn:Close()
					clients[fd] = nil 
				end
			end)
		end
	end)

	local timer1 = event_loop:RegTimer(1000,function ()
		collectgarbage("collect")
		print(client_count,packet_count)
		packet_count = 0
	end)

	if server then
		event_loop:Run()
	end

##broadcast_cli.lua

	local chuck = require("chuck")
	local socket = chuck.socket
	local packet = require("packet")

	local event_loop = chuck.event_loop.New()

	local connections = {}
	local packet_count = 0

	for i=1,500 do
		socket.stream.ip4.dail(event_loop,"127.0.0.1",8010,function (fd)
			local conn = socket.stream.New(fd,65536,packet.Decoder())
			if conn then
			connections[fd] = conn
			conn:Bind(event_loop,function (data)
					if data then 
						packet_count = packet_count + 1
					else
						print("client disconnected") 
						conn:Close()
						connections[fd] = nil 
					end
				end)
			end
		end)
	end

	local timer1 = event_loop:RegTimer(1000,function ()
		print(packet_count)
		collectgarbage("collect")
		packet_count = 0
	end)

	local timer2 = event_loop:RegTimer(300,function ()
		for k,v in pairs(connections) do
			local buff = chuck.buffer.New()
			local w = packet.Writer(buff)
			w:WriteStr("hello")
			v:Send(buff)
		end
	end)

	event_loop:Run()


#customer


![](img/20150811163353.jpg "福州市欢乐互动科技有限公司")
