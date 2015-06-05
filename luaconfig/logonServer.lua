SERVER_TYPE_LOGON = 11

SERVER_ID_LOGON = 10011
SERVER_ID_HUB = 10021

SERVER_IP = "127.0.0.1"

function getLogonServerConfig()
	local config = {id=SERVER_TYPE_LOGON, ttype=SERVER_ID_LOGON, ip=SERVER_IP, port=SERVER_ID_LOGON, threads=4}
	return config
end

function getHubServerIpPort()
	local config = {ip=SERVER_IP, port=SERVER_ID_HUB}
	return config
end
