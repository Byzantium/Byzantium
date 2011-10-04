BEGIN TRANSACTION;
CREATE TABLE wired (enabled TEXT, configured TEXT, gateway TEXT, interface TEXT, ipaddress TEXT, netmask TEXT);
CREATE TABLE wireless (client_netmask TEXT, client_ip TEXT, client_interface TEXT, enabled TEXT, channel NUMERIC, configured TEXT, essid TEXT, mesh_interface TEXT, mesh_ip TEXT, mesh_netmask TEXT);
COMMIT;
