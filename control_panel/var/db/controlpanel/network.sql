BEGIN TRANSACTION;
CREATE TABLE wired (enabled TEXT, configured TEXT, gateway TEXT, interface TEXT, ipaddress TEXT, netmask TEXT);
CREATE TABLE wireless (enabled TEXT, channel NUMERIC, configured TEXT, essid TEXT, interface TEXT, ipaddress TEXT, netmask TEXT);
COMMIT;
