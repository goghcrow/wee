<?php

define("TH_FIN", 0x01);
define("TH_SYN", 0x02);
define("TH_RST", 0x04);
define("TH_PUSH",0x08);
define("TH_ACK", 0x10);
define("TH_URG", 0x20);

tcpsniff("lo0", "tcp and port 9999", function($pktHdr, $ipHdr, $tcpHdr, $payload) {
	$tcpFlags = $tcpHdr["th_flags"];
	if ($tcpFlags & TH_FIN) {
		echo "FIN\n";
	} else if ($tcpFlags & TH_SYN) {
		echo "SYN\n";
	} else if ($tcpFlags & TH_RST) {
		echo "RST\n";
	} else if ($tcpFlags & TH_PUSH) {
		echo "PSH\n";
	} else if ($tcpFlags & TH_ACK) {
		echo "ACK\n";
	}

	// if ($tcpFlags & TH_FIN || $tcpFlags & TH_RST) {
	// 	var_dump("CLOSED");
	// }

	// var_dump(func_get_args());
	if ($payload != "") {
		var_dump($payload);		
	}
});
