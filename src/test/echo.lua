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
