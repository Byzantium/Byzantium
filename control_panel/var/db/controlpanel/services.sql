BEGIN TRANSACTION;
CREATE TABLE daemons (showtouser TEXT, port NUMERIC, initscript TEXT, name TEXT, status TEXT);
INSERT INTO daemons VALUES('no',6667,'rc.ngircd','IRC server','disabled');
INSERT INTO daemons VALUES('no',22,'rc.sshd','SSH','disabled');
INSERT INTO daemons VALUES('yes',9090,'rc.qwebirc','chat','disabled');
INSERT INTO daemons VALUES('yes',9001,'rc.etherpad-lite','pad','disabled');
CREATE TABLE webapps (name TEXT, status TEXT);
INSERT INTO webapps VALUES('microblog','disabled');
COMMIT;
