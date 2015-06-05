SERVER_TYPE_HUB = 21

SERVER_ID_HUB = 10021

SERVER_IP = "127.0.0.1"

function getHubServerConfig()
	local config = {id=SERVER_TYPE_HUB, ttype=SERVER_ID_HUB, ip=SERVER_IP, port=SERVER_ID_HUB, threads=1}
	return config
end
