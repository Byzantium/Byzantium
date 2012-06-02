BEGIN TRANSACTION;
CREATE TABLE daemons (port NUMERIC, initscript TEXT, name TEXT, status TEXT);
INSERT INTO daemons VALUES(6667,'rc.ngircd','IRC server','disabled');
INSERT INTO daemons VALUES(22,'rc.sshd','SSH','disabled');
INSERT INTO daemons VALUES(9090,'rc.qwebirc','chat','disabled');
INSERT INTO daemons VALUES(9001,'rc.etherpad-lite','pad','disabled');
CREATE TABLE webapps (name TEXT, status TEXT);
INSERT INTO webapps VALUES('microblog','disabled');
COMMIT;
