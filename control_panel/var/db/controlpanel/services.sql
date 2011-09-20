BEGIN TRANSACTION;
CREATE TABLE daemons (initscript TEXT, name TEXT, status TEXT);
CREATE TABLE webapps (name TEXT, status TEXT);
COMMIT;
