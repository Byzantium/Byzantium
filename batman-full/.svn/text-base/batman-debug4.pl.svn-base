#!/usr/bin/perl

use strict;
use utf8;


my ( %myself_hash, %receive_hash, %forward_hash, %sum_hash, $last_orig, $last_neigh, $last_seq, $last_ttl, $orig_interval, $total_seconds, %to_do_hash, %seq_hash, $rt_table );

$orig_interval = 1000;


if ( ( $ARGV[0] ne "-s" ) && ( $ARGV[0] ne "-p" ) && ( $ARGV[0] ne "-r" ) ) {

	print "Usage: batman-debug4.pl option <file>\n";
	print "\t-p    packet statistic\n";
	print "\t-s    sequence number statistic\n";
	print "\t-r    routing statistic\n";
	exit;

}

if ( ! -e $ARGV[1] ) {

	print "B.A.T.M.A.N log file could not be found: $ARGV[1]\n";
	exit;

}

$rt_table = 0;

open(BATMANLOG, "< $ARGV[1]");

while ( <BATMANLOG> ) {

	if ( $rt_table ) {

		if ( m/----------------------------------------------\ END\ DEBUG/ ) {

			$rt_table = 0;
			sleep(2);

		} else {

			print $_

		}

	} else {

		if ( m/Received\ BATMAN\ packet\ via\ NB:\ ([\d]+\.[\d]+\.[\d]+\.[\d]+), IF: (.*?) ([\d]+\.[\d]+\.[\d]+\.[\d]+)\ \(from OG:\ ([\d]+\.[\d]+\.[\d]+\.[\d]+),\ via\ old\ OG:\ ([\d]+\.[\d]+\.[\d]+\.[\d]+),\ seqno ([\d]+),\ tq ([\d]+),\ TTL ([\d]+)/ ) {

			$receive_hash{ $4 }{ $1 }{ "num_recv" }++;
			$myself_hash{ $3 }{ "if" } = $2;
			$last_orig = $4;
			$last_neigh = $1;
			$last_seq = $6;
			$last_ttl = $8;

		} elsif ( m/Forwarding\ packet\ \(originator\ ([\d]+\.[\d]+\.[\d]+\.[\d]+),\ seqno ([\d]+),\ TTL\ ([\d]+)/ ) {

			if ( $1 eq $last_orig ) {

	# 			$receive_hash{ $last_orig }{ $last_neigh }{ "num_forw" }++;

 			} elsif ( ( $myself_hash{ $1 } ) || ($3 == 50)) {

				$myself_hash{ $1 }{ "sent" }++;

			} else {

				print "Not equal: $_ <> '$1' '$last_orig'\n";

			}

		} elsif ( m/Drop\ packet:/ ) {

			$receive_hash{ $last_orig }{ $last_neigh }{ "num_drop" }++;

			if ( m/incompatible\ batman\ version/ ) {

				$receive_hash{ $last_orig }{ $last_neigh }{ "version" }++;

			} elsif ( m/received\ my\ own\ broadcast/ ) {

				$receive_hash{ $last_orig }{ $last_neigh }{ "own_broad" }++;

			} elsif ( m/ignoring\ all\ packets\ with\ broadcast\ source\ IP/ ) {

				$receive_hash{ $last_orig }{ $last_neigh }{ "bcast_src" }++;

			} elsif ( m/originator\ packet\ from\ myself/ ) {

				$receive_hash{ $last_orig }{ $last_neigh }{ "own_rebroad" }++;

			} elsif ( m/originator\ packet\ with\ tq\ is\ 0/ ) {

				$receive_hash{ $last_orig }{ $last_neigh }{ "tq_zero" }++;

			} elsif ( m/originator\ packet\ with\ unidirectional\ flag/ ) {

				$receive_hash{ $last_orig }{ $last_neigh }{ "uni_flag" }++;

			} elsif ( m/OGM\ via\ unknown\ neighbor/ ) {

				$receive_hash{ $last_orig }{ $last_neigh }{ "unknown" }++;

			} elsif ( m/ignoring\ all\ rebroadcast\ echos/ ) {

				$receive_hash{ $last_orig }{ $last_neigh }{ "bcast_echo" }++;

			} elsif ( m/\ not\ received\ via\ bidirectional\ link/ ) {

				$receive_hash{ $last_orig }{ $last_neigh }{ "uni_link" }++;

			} elsif ( m/BNTOG:[\ ]+NO/ ) {

				$receive_hash{ $last_orig }{ $last_neigh }{ "bntog" }++;

			} elsif ( m/duplicate\ packet/ ) {

				$receive_hash{ $last_orig }{ $last_neigh }{ "dup" }++;

			} else {

				print "Unknown drop: $_ \n";
				exit;

			}

		} elsif ( m/Forward packet:/ ) {

			$forward_hash{ $last_orig }{ $last_neigh }{ "num_forw" }++;

			if ( m/rebroadcast\ neighbour\ packet\ with\ direct\ link\ flag/ ) {

				$forward_hash{ $last_orig }{ $last_neigh }{ "direct_link" }++;

			} elsif ( m/rebroadcast\ neighbour\ packet\ with\ direct\ link\ and\ unidirectional\ flag/ ) {

				$forward_hash{ $last_orig }{ $last_neigh }{ "direct_uni" }++;

			} elsif ( m/rebroadcast\ originator\ packet/ ) {

				$forward_hash{ $last_orig }{ $last_neigh }{ "rebroad" }++;

			} elsif ( m/duplicate\ packet\ received\ via\ best\ neighbour\ with\ best\ ttl/ ) {

				$forward_hash{ $last_orig }{ $last_neigh }{ "dup" }++;

			} else {

				print "Unknown forward: $_ \n";
				exit;

			}

		} elsif ( m/update_originator/ ) {

			push( @{ $seq_hash{ $last_orig }{ $last_neigh } }, "$last_seq [$last_ttl]" );

		} elsif ( m/ttl\ exceeded/ ) {

			$receive_hash{ $last_orig }{ $last_neigh }{ "ttl" }++;

		} elsif ( m/Using\ interface\ (.*?)\ with\ address\ ([\d]+\.[\d]+\.[\d]+\.[\d]+)/ ) {

			$myself_hash{ $2 }{ "if" } = $1;

		} elsif ( m/orginator interval: ([\d]+)/ ) {

			$orig_interval = $1;

		} elsif ( m/Originator\ list/ ) {

			if ( $ARGV[0] eq "-r" ) {

				$rt_table = 1;
				system( "clear" );

			}

		} elsif ( m/\[.*\](.*)/ ) {

			$to_do_hash{ $1 }++;

		}

	}

}


close( BATMANLOG );


if ( $ARGV[0] eq "-p" ) {

	print "\nSent:\n^^^^\n";

	foreach my $my_ip ( keys %myself_hash ) {

		$total_seconds = ( $myself_hash{ $my_ip }{ "sent" } * $orig_interval ) / 1000;
		print " => $my_ip (" . $myself_hash{ $my_ip }{ "if" } . "): send " . $myself_hash{ $my_ip }{ "sent" } . " packets in $total_seconds seconds\n";

	}


	print "\n\nReceived:\n^^^^^^^^";

	foreach my $orginator ( keys %receive_hash ) {

		my ($sum, $sum_dropped, $sum_forw, $sum_ver, $sum_own_broad, $sum_rebroad, $sum_bcast_echo, $sum_tq_zero, $sum_uni_flag, $sum_unknown, $sum_uni_link, $sum_bntog, $sum_bcast_src, $sum_dup, $sum_ttl);
		my $string = "";

		$sum = 0, $sum_dropped = 0, $sum_forw = 0, $sum_ver = 0, $sum_own_broad = 0, $sum_rebroad = 0, $sum_bcast_echo = 0, $sum_tq_zero = 0, $sum_uni_flag = 0, $sum_unknown = 0, $sum_uni_link = 0, $sum_bntog = 0, $sum_bcast_src = 0, $sum_dup = 0, $sum_ttl = 0;

		foreach my $neighbour ( keys %{ $receive_hash{ $orginator } } ) {

			$sum += $receive_hash{ $orginator }{ $neighbour }{ "num_recv" };
			$string .= " => $neighbour" . ( $myself_hash{ $neighbour } ? " (myself):\t" : ": \t\t" );
			$string .= " recv = " . $receive_hash{ $orginator }{ $neighbour }{ "num_recv" };
			$string .= " <> forw = " . ( $forward_hash{ $orginator }{ $neighbour }{ "num_forw" } ? $forward_hash{ $orginator }{ $neighbour }{ "num_forw" } : "0" );
			$string .= " <> drop = " . ( $receive_hash{ $orginator }{ $neighbour }{ "num_drop" } ? $receive_hash{ $orginator }{ $neighbour }{ "num_drop" } : "0" );
			$string .= " \t [";
			$string .= ( $receive_hash{ $orginator }{ $neighbour }{ "version" } ? "version = " . $receive_hash{ $orginator }{ $neighbour }{ "version" } . "; "  : "" );
			$string .= ( $receive_hash{ $orginator }{ $neighbour }{ "own_broad" } ? "own_broad = " . $receive_hash{ $orginator }{ $neighbour }{ "own_broad" } . "; "  : "" );
			$string .= ( $receive_hash{ $orginator }{ $neighbour }{ "own_rebroad" } ? "own_rebroad = " . $receive_hash{ $orginator }{ $neighbour }{ "own_rebroad" } . "; "  : "" );
			$string .= ( $receive_hash{ $orginator }{ $neighbour }{ "bcast_echo" } ? "bcast_echo = " . $receive_hash{ $orginator }{ $neighbour }{ "bcast_echo" } . "; "  : "" );
			$string .= ( $receive_hash{ $orginator }{ $neighbour }{ "tq_zero" } ? "tq_zero = " . $receive_hash{ $orginator }{ $neighbour }{ "tq_zero" } . "; "  : "" );
			$string .= ( $receive_hash{ $orginator }{ $neighbour }{ "uni_flag" } ? "uni_flag = " . $receive_hash{ $orginator }{ $neighbour }{ "uni_flag" } . "; "  : "" );
			$string .= ( $receive_hash{ $orginator }{ $neighbour }{ "unknown" } ? "unknown = " . $receive_hash{ $orginator }{ $neighbour }{ "unknown" } . "; "  : "" );
			$string .= ( $receive_hash{ $orginator }{ $neighbour }{ "uni_link" } ? "uni_link = " . $receive_hash{ $orginator }{ $neighbour }{ "uni_link" } . "; "  : "" );
			$string .= ( $receive_hash{ $orginator }{ $neighbour }{ "bntog" } ? "btnog = " . $receive_hash{ $orginator }{ $neighbour }{ "bntog" } . "; "  : "" );
			$string .= ( $receive_hash{ $orginator }{ $neighbour }{ "bcast_src" } ? "bcast_src = " . $receive_hash{ $orginator }{ $neighbour }{ "bcast_src" } . "; "  : "" );
			$string .= ( $receive_hash{ $orginator }{ $neighbour }{ "dup" } ? "dup = " . $receive_hash{ $orginator }{ $neighbour }{ "dup" } . "; "  : "" );
			$string .= ( $receive_hash{ $orginator }{ $neighbour }{ "ttl" } ? "ttl = " . $receive_hash{ $orginator }{ $neighbour }{ "ttl" } . "; "  : "" );
			$string .= "] [";
			$string .= ( $forward_hash{ $orginator }{ $neighbour }{ "direct_link" } ? "direct_link = " . $forward_hash{ $orginator }{ $neighbour }{ "direct_link" } . "; "  : "" );
			$string .= ( $forward_hash{ $orginator }{ $neighbour }{ "direct_uni" } ? "direct_uni = " . $forward_hash{ $orginator }{ $neighbour }{ "direct_uni" } . "; "  : "" );
			$string .= ( $forward_hash{ $orginator }{ $neighbour }{ "rebroad" } ? "rebroad = " . $forward_hash{ $orginator }{ $neighbour }{ "rebroad" } . "; "  : "" );
			$string .= ( $forward_hash{ $orginator }{ $neighbour }{ "dup" } ? "dup = " . $forward_hash{ $orginator }{ $neighbour }{ "dup" } . "; "  : "" );
			$string .= "]\n";

			$sum_dropped += $receive_hash{ $orginator }{ $neighbour }{ "num_drop" } if $receive_hash{ $orginator }{ $neighbour }{ "num_drop" };
			$sum_forw += $forward_hash{ $orginator }{ $neighbour }{ "num_forw" } if $forward_hash{ $orginator }{ $neighbour }{ "num_forw" };

			$sum_ver += $receive_hash{ $orginator }{ $neighbour }{ "version" } if $receive_hash{ $orginator }{ $neighbour }{ "version" };
			$sum_own_broad += $receive_hash{ $orginator }{ $neighbour }{ "own_broad" } if $receive_hash{ $orginator }{ $neighbour }{ "own_broad" };
			$sum_rebroad += $receive_hash{ $orginator }{ $neighbour }{ "own_rebroad" } if $receive_hash{ $orginator }{ $neighbour }{ "own_rebroad" };
			$sum_bcast_echo += $receive_hash{ $orginator }{ $neighbour }{ "bcast_echo" } if $receive_hash{ $orginator }{ $neighbour }{ "bcast_echo" };
			$sum_tq_zero += $receive_hash{ $orginator }{ $neighbour }{ "tq_zero" } if $receive_hash{ $orginator }{ $neighbour }{ "tq_zero" };
			$sum_uni_flag += $receive_hash{ $orginator }{ $neighbour }{ "uni_flag" } if $receive_hash{ $orginator }{ $neighbour }{ "uni_flag" };
			$sum_unknown += $receive_hash{ $orginator }{ $neighbour }{ "unknown" } if $receive_hash{ $orginator }{ $neighbour }{ "unknown" };
			$sum_uni_link += $receive_hash{ $orginator }{ $neighbour }{ "uni_link" } if $receive_hash{ $orginator }{ $neighbour }{ "uni_link" };
			$sum_bntog += $receive_hash{ $orginator }{ $neighbour }{ "bntog" } if $receive_hash{ $orginator }{ $neighbour }{ "bntog" };
			$sum_bcast_src += $receive_hash{ $orginator }{ $neighbour }{ "bcast_src" } if $receive_hash{ $orginator }{ $neighbour }{ "bcast_src" };
			$sum_dup += $receive_hash{ $orginator }{ $neighbour }{ "dup" } if $receive_hash{ $orginator }{ $neighbour }{ "dup" };
			$sum_ttl += $receive_hash{ $orginator }{ $neighbour }{ "ttl" } if $receive_hash{ $orginator }{ $neighbour }{ "ttl" };

		}

		print "\norginator $orginator" . ( $myself_hash{ $orginator } ? " (myself)" : "" ) . ": total recv = $sum\n";
		print $string;

		$sum_hash{"num_orig"}++;
		$sum_hash{"total_recv"} += $sum;
		$sum_hash{"total_dropped"} += $sum_dropped;
		$sum_hash{"total_forw"} += $sum_forw;
		$sum_hash{"drop"}{"version"} += $sum_ver;
		$sum_hash{"drop"}{"own_broad"} += $sum_own_broad;
		$sum_hash{"drop"}{"own_rebroad"} += $sum_rebroad;
		$sum_hash{"drop"}{"bcast_echo"} += $sum_bcast_echo;
		$sum_hash{"drop"}{"tq_zero"} += $sum_tq_zero;
		$sum_hash{"drop"}{"uni_flag"} += $sum_uni_flag;
		$sum_hash{"drop"}{"unknown"} += $sum_unknown;
		$sum_hash{"drop"}{"uni_link"} += $sum_uni_link;
		$sum_hash{"drop"}{"bntog"} += $sum_bntog;
		$sum_hash{"drop"}{"bcast_src"} += $sum_bcast_src;
		$sum_hash{"drop"}{"dup"} += $sum_dup;
		$sum_hash{"drop"}{"ttl"} += $sum_ttl;

	}

	print "\n\nSummary:\n^^^^^^^\n";
	print "num originators: $sum_hash{'num_orig'}\n";
	print "total recv: $sum_hash{'total_recv'}\n";
	printf("total dropped: $sum_hash{'total_dropped'} (%.2f%)\n", $sum_hash{'total_dropped'} * 100 / $sum_hash{'total_recv'});
	printf("total forwarded: $sum_hash{'total_forw'} (%.2f%)\n", $sum_hash{'total_forw'} * 100 / $sum_hash{'total_recv'});

	foreach my $drop (keys %{$sum_hash{"drop"}}) {
		printf("drop reason: $drop, num drops: $sum_hash{'drop'}{$drop} (%.2f%)\n", $sum_hash{'drop'}{$drop} * 100 / $sum_hash{'total_dropped'}) if ($sum_hash{'drop'}{$drop} != 0);
	}

	print "\n\nHelp:\n^^^^\n";
	print " Dropped packets:\n";
	print "\tversion     = received packet indicated incompatible batman version\n";
	print "\town_broad   = received my own broadcast\n";
	print "\town_rebroad = received rebroadcast of my packet via neighbour\n";
	print "\tbcast_echo  = received rebroadcast of a packet I already rebroadcasted\n";
	print "\ttq_zero  = received rebroadcast with a tq of zero\n";
	print "\tuni_flag    = received packet with unidrectional flag\n";
	print "\tunknown     = received packet via unknown neighbour\n";
	print "\tuni_link    = received packet via unidirectional link\n";
	print "\tbntog       = packet not received via best neighbour\n";
	print "\tbcast_src   = sender address is a broadcast address\n";
	print "\tdup         = received packet is a duplicate\n";
	print "\tttl         = ttl of packet exceeded\n\n";

	print " Forwarded packets:\n";
	print "\tdirect_link = forwarded packet with direct_link flag\n";
	print "\tdirect_uni  = forwarded packet with direct_link and unidirectional flag\n";
	print "\trebroad     = just rebroadcast packet\n";
	print "\tdup         = rebroadcast packet allthough it is a duplicate\n";

} elsif ( $ARGV[0] eq "-s" ) {

	foreach my $orginator ( keys %seq_hash ) {

		print "\n\nOriginator: $orginator\n^^^^^^^^^\n";

		foreach my $neighbour ( keys %{ $seq_hash{ $orginator } } ) {

			print "\nNeighbour: $neighbour\n";

			foreach my $seqno ( @{ $seq_hash{ $orginator }{ $neighbour } } ) {

				print "$seqno ";

			}

			print "\n";

		}

	}

}



# foreach my $todo ( keys %to_do_hash ) {
#
# 	print "ToDo: $todo -> $to_do_hash{ $todo }\n" if ( $to_do_hash{ $todo } > 2 );
#
# }
