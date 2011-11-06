<?php

// $Id$

require_once("../config.php");

// $_REQUEST['url'] = original url
// $_REQUEST['gw_address'] = address for response
// $_REQUEST['gw_port'] = port for response
// $token = "RAND";

$token = abs(rand(1000000000,4200000000));

$template = file_get_contents($BASE_PATH."/templates/login.temp");

$template = preg_replace('/[%][%]BUTTON=(.*)[%][%]/i', "<form method='GET' action='http://$_REQUEST[gw_address]:$_REQUEST[gw_port]/wifidog/auth'><input type='hidden' name='token' value='$token' /><input type='submit' value='\\1' /></form>", $template);

$template = preg_replace('/[%][%]REFRESH[%][%]/i', "<meta http-equiv='Refresh' content='30; URL=http://$_REQUEST[gw_address]:$_REQUEST[gw_port]/wifidog/auth?token=$token' />", $template);

echo $template;
?>
