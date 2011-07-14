BEGIN TRANSACTION;
CREATE TABLE wired (configured TEXT, gateway TEXT, interface TEXT, ipaddress TEXT, netmask TEXT);
CREATE TABLE wireless (channel NUMERIC, configured TEXT, essid TEXT, interface TEXT, ipaddress TEXT, netmask TEXT);
COMMIT;
