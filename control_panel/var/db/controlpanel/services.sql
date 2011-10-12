BEGIN TRANSACTION;
CREATE TABLE daemons (initscript TEXT, name TEXT, status TEXT);
INSERT INTO daemons VALUES('rc.ngircd','IRC server','disabled');
INSERT INTO daemons VALUES('rc.sshd','SSH','disabled');
INSERT INTO daemons VALUES('rc.etherpad-lite','Pad','disabled');
INSERT INTO daemons VALUES('rc.qwebirc','Web Chat','disabled');
CREATE TABLE webapps (name TEXT, status TEXT);
INSERT INTO webapps VALUES('microblog','disabled');
COMMIT;
