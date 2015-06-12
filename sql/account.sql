CREATE TABLE `ACCOUNT`      (
	`UID`    int(10) unsigned NOT NULL auto_increment,
	`UNAME`  varchar(33) NOT NULL default '',
	`PASSWD` varchar(33) NOT NULL default '',
	PRIMARY KEY (`UID`)
) ENGINE=InnoDB;

INSERT INTO `ACCOUNT` VALUES ('1', '1', '123456');
