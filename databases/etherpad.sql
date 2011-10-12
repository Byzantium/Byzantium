-- MySQL dump 10.13  Distrib 5.1.56, for slackware-linux-gnu (i486)
--
-- Host: localhost    Database: etherpad
-- ------------------------------------------------------
-- Server version	5.1.56

/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET @OLD_CHARACTER_SET_RESULTS=@@CHARACTER_SET_RESULTS */;
/*!40101 SET @OLD_COLLATION_CONNECTION=@@COLLATION_CONNECTION */;
/*!40101 SET NAMES utf8 */;
/*!40103 SET @OLD_TIME_ZONE=@@TIME_ZONE */;
/*!40103 SET TIME_ZONE='+00:00' */;
/*!40014 SET @OLD_UNIQUE_CHECKS=@@UNIQUE_CHECKS, UNIQUE_CHECKS=0 */;
/*!40014 SET @OLD_FOREIGN_KEY_CHECKS=@@FOREIGN_KEY_CHECKS, FOREIGN_KEY_CHECKS=0 */;
/*!40101 SET @OLD_SQL_MODE=@@SQL_MODE, SQL_MODE='NO_AUTO_VALUE_ON_ZERO' */;
/*!40111 SET @OLD_SQL_NOTES=@@SQL_NOTES, SQL_NOTES=0 */;

--
-- Table structure for table `store`
--

DROP TABLE IF EXISTS `store`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `store` (
  `key` varchar(100) NOT NULL,
  `value` longtext NOT NULL,
  PRIMARY KEY (`key`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `store`
--

LOCK TABLES `store` WRITE;
/*!40000 ALTER TABLE `store` DISABLE KEYS */;
INSERT INTO `store` VALUES ('globalAuthor:a.LPkMtZXg5cZ1mA9A','{\"colorId\":\"#c78fff\",\"name\":\"undefined\",\"timestamp\":1317321645001}'),('globalAuthor:a.odBxhsJ1yoYqeCvG','{\"colorId\":\"#a9d9b5\",\"name\":\"undefined\",\"timestamp\":1317321666108}'),('pad2readonly:y5ADyzB7pJ','\"r.sUQDy10kAgXUbcmI\"'),('pad:y5ADyzB7pJ','{\"atext\":{\"text\":\"Let\'s try this again, this time with MySQL on the back end!\\n\\nI can see it from here.\\n\\nAnd I can see you from here!\\n\\n\\\\o/\\n\\n\",\"attribs\":\"*0+1n|1+1*1|1+1*1+n|1+1*0|2+u|1+1*1+3|2+2\"},\"pool\":{\"numToAttrib\":{\"0\":[\"author\",\"a.LPkMtZXg5cZ1mA9A\"],\"1\":[\"author\",\"a.odBxhsJ1yoYqeCvG\"]},\"nextNum\":2},\"head\":35,\"chatHead\":-1,\"publicStatus\":false,\"passwordHash\":null}'),('pad:y5ADyzB7pJ:revs:0','{\"changeset\":\"Z:1>6b|5+6b$Welcome to Etherpad Lite!\\n\\nThis pad text is synchronized as you type, so that everyone viewing this page sees the same text. This allows you to collaborate seamlessly on documents!\\n\\nEtherpad Lite on Github: http://j.mp/ep-lite\\n\",\"meta\":{\"author\":\"\",\"timestamp\":1317321645019,\"atext\":{\"text\":\"Welcome to Etherpad Lite!\\n\\nThis pad text is synchronized as you type, so that everyone viewing this page sees the same text. This allows you to collaborate seamlessly on documents!\\n\\nEtherpad Lite on Github: http://j.mp/ep-lite\\n\\n\",\"attribs\":\"|6+6c\"}}}'),('pad:y5ADyzB7pJ:revs:1','{\"changeset\":\"Z:6c<69|4-52-18*0+1$L\",\"meta\":{\"author\":\"a.LPkMtZXg5cZ1mA9A\",\"timestamp\":1317321650707}}'),('pad:y5ADyzB7pJ:revs:10','{\"changeset\":\"Z:1f>2=1d*0+2$ba\",\"meta\":{\"author\":\"a.LPkMtZXg5cZ1mA9A\",\"timestamp\":1317321655250}}'),('pad:y5ADyzB7pJ:revs:11','{\"changeset\":\"Z:1h<3=1c-3$\",\"meta\":{\"author\":\"a.LPkMtZXg5cZ1mA9A\",\"timestamp\":1317321655755}}'),('pad:y5ADyzB7pJ:revs:12','{\"changeset\":\"Z:1e<3=19-3$\",\"meta\":{\"author\":\"a.LPkMtZXg5cZ1mA9A\",\"timestamp\":1317321656251}}'),('pad:y5ADyzB7pJ:revs:13','{\"changeset\":\"Z:1b>8=19*0+8$ the bac\",\"meta\":{\"author\":\"a.LPkMtZXg5cZ1mA9A\",\"timestamp\":1317321656753}}'),('pad:y5ADyzB7pJ:revs:14','{\"changeset\":\"Z:1j>4=1h*0+4$ end\",\"meta\":{\"author\":\"a.LPkMtZXg5cZ1mA9A\",\"timestamp\":1317321657424}}'),('pad:y5ADyzB7pJ:revs:15','{\"changeset\":\"Z:1n<4=1h-4$\",\"meta\":{\"author\":\"a.LPkMtZXg5cZ1mA9A\",\"timestamp\":1317321658080}}'),('pad:y5ADyzB7pJ:revs:16','{\"changeset\":\"Z:1j>5=1h*0+5$k end\",\"meta\":{\"author\":\"a.LPkMtZXg5cZ1mA9A\",\"timestamp\":1317321658584}}'),('pad:y5ADyzB7pJ:revs:17','{\"changeset\":\"Z:1o>1=1m*0+1$!\",\"meta\":{\"author\":\"a.LPkMtZXg5cZ1mA9A\",\"timestamp\":1317321659086}}'),('pad:y5ADyzB7pJ:revs:18','{\"changeset\":\"Z:1p>1|1=1o*1|1+1$\\n\",\"meta\":{\"author\":\"a.odBxhsJ1yoYqeCvG\",\"timestamp\":1317321668282}}'),('pad:y5ADyzB7pJ:revs:19','{\"changeset\":\"Z:1q>3|2=1p*1+3$I c\",\"meta\":{\"author\":\"a.odBxhsJ1yoYqeCvG\",\"timestamp\":1317321668949}}'),('pad:y5ADyzB7pJ:revs:2','{\"changeset\":\"Z:3>6=1*0+6$et\'s t\",\"meta\":{\"author\":\"a.LPkMtZXg5cZ1mA9A\",\"timestamp\":1317321651137}}'),('pad:y5ADyzB7pJ:revs:20','{\"changeset\":\"Z:1t>1|2=1p=3|1+1$\\n\",\"meta\":{\"author\":\"\",\"timestamp\":1317321668950}}'),('pad:y5ADyzB7pJ:revs:21','{\"changeset\":\"Z:1u>5|2=1p=3*1+5$an se\",\"meta\":{\"author\":\"a.odBxhsJ1yoYqeCvG\",\"timestamp\":1317321669602}}'),('pad:y5ADyzB7pJ:revs:22','{\"changeset\":\"Z:1z>f|2=1p=8*1+f$e it from here.\",\"meta\":{\"author\":\"a.odBxhsJ1yoYqeCvG\",\"timestamp\":1317321671000}}'),('pad:y5ADyzB7pJ:revs:23','{\"changeset\":\"Z:2e>1|3=2d*0|1+1$\\n\",\"meta\":{\"author\":\"a.LPkMtZXg5cZ1mA9A\",\"timestamp\":1317321673444}}'),('pad:y5ADyzB7pJ:revs:24','{\"changeset\":\"Z:2f>1|4=2e*0+1$A\",\"meta\":{\"author\":\"a.LPkMtZXg5cZ1mA9A\",\"timestamp\":1317321673836}}'),('pad:y5ADyzB7pJ:revs:25','{\"changeset\":\"Z:2g>1|4=2e=1|1+1$\\n\",\"meta\":{\"author\":\"\",\"timestamp\":1317321673839}}'),('pad:y5ADyzB7pJ:revs:26','{\"changeset\":\"Z:2h>7|4=2e=1*0+7$nd I ca\",\"meta\":{\"author\":\"a.LPkMtZXg5cZ1mA9A\",\"timestamp\":1317321674341}}'),('pad:y5ADyzB7pJ:revs:27','{\"changeset\":\"Z:2o>4|4=2e=8*0+4$n se\",\"meta\":{\"author\":\"a.LPkMtZXg5cZ1mA9A\",\"timestamp\":1317321674838}}'),('pad:y5ADyzB7pJ:revs:28','{\"changeset\":\"Z:2s>6|4=2e=c*0+6$e you \",\"meta\":{\"author\":\"a.LPkMtZXg5cZ1mA9A\",\"timestamp\":1317321675340}}'),('pad:y5ADyzB7pJ:revs:29','{\"changeset\":\"Z:2y>7|4=2e=i*0+7$from he\",\"meta\":{\"author\":\"a.LPkMtZXg5cZ1mA9A\",\"timestamp\":1317321675845}}'),('pad:y5ADyzB7pJ:revs:3','{\"changeset\":\"Z:9>6=7*0+6$ry thi\",\"meta\":{\"author\":\"a.LPkMtZXg5cZ1mA9A\",\"timestamp\":1317321651647}}'),('pad:y5ADyzB7pJ:revs:30','{\"changeset\":\"Z:35>3|4=2e=p*0+3$re!\",\"meta\":{\"author\":\"a.LPkMtZXg5cZ1mA9A\",\"timestamp\":1317321676341}}'),('pad:y5ADyzB7pJ:revs:31','{\"changeset\":\"Z:38>1|4=2e=s*0|1+1$\\n\",\"meta\":{\"author\":\"a.LPkMtZXg5cZ1mA9A\",\"timestamp\":1317321676889}}'),('pad:y5ADyzB7pJ:revs:32','{\"changeset\":\"Z:39>1|6=38*1+1$\\\\\",\"meta\":{\"author\":\"a.odBxhsJ1yoYqeCvG\",\"timestamp\":1317321680171}}'),('pad:y5ADyzB7pJ:revs:33','{\"changeset\":\"Z:3a>1|6=38=1|1+1$\\n\",\"meta\":{\"author\":\"\",\"timestamp\":1317321680171}}'),('pad:y5ADyzB7pJ:revs:34','{\"changeset\":\"Z:3b>1|6=38=1*1+1$o\",\"meta\":{\"author\":\"a.odBxhsJ1yoYqeCvG\",\"timestamp\":1317321680671}}'),('pad:y5ADyzB7pJ:revs:35','{\"changeset\":\"Z:3c>1|6=38=2*1+1$/\",\"meta\":{\"author\":\"a.odBxhsJ1yoYqeCvG\",\"timestamp\":1317321681168}}'),('pad:y5ADyzB7pJ:revs:4','{\"changeset\":\"Z:f>7=d*0+7$s again\",\"meta\":{\"author\":\"a.LPkMtZXg5cZ1mA9A\",\"timestamp\":1317321652143}}'),('pad:y5ADyzB7pJ:revs:5','{\"changeset\":\"Z:m>7=k*0+7$, this \",\"meta\":{\"author\":\"a.LPkMtZXg5cZ1mA9A\",\"timestamp\":1317321652734}}'),('pad:y5ADyzB7pJ:revs:6','{\"changeset\":\"Z:t>9=r*0+9$time with\",\"meta\":{\"author\":\"a.LPkMtZXg5cZ1mA9A\",\"timestamp\":1317321653246}}'),('pad:y5ADyzB7pJ:revs:7','{\"changeset\":\"Z:12>3=10*0+3$ My\",\"meta\":{\"author\":\"a.LPkMtZXg5cZ1mA9A\",\"timestamp\":1317321653746}}'),('pad:y5ADyzB7pJ:revs:8','{\"changeset\":\"Z:15>4=13*0+4$SQL \",\"meta\":{\"author\":\"a.LPkMtZXg5cZ1mA9A\",\"timestamp\":1317321654248}}'),('pad:y5ADyzB7pJ:revs:9','{\"changeset\":\"Z:19>6=17*0+6$onthe \",\"meta\":{\"author\":\"a.LPkMtZXg5cZ1mA9A\",\"timestamp\":1317321654748}}'),('readonly2pad:r.sUQDy10kAgXUbcmI','\"y5ADyzB7pJ\"'),('token2author:t.NWmQQvDDZZfBz311VGaR','\"a.LPkMtZXg5cZ1mA9A\"'),('token2author:t.R3FtMNWJIG55TPGNAZ9y','\"a.odBxhsJ1yoYqeCvG\"');
/*!40000 ALTER TABLE `store` ENABLE KEYS */;
UNLOCK TABLES;
/*!40103 SET TIME_ZONE=@OLD_TIME_ZONE */;

/*!40101 SET SQL_MODE=@OLD_SQL_MODE */;
/*!40014 SET FOREIGN_KEY_CHECKS=@OLD_FOREIGN_KEY_CHECKS */;
/*!40014 SET UNIQUE_CHECKS=@OLD_UNIQUE_CHECKS */;
/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
/*!40111 SET SQL_NOTES=@OLD_SQL_NOTES */;

-- Dump completed on 2011-10-12 22:07:51
