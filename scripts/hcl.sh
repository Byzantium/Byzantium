#!/bin/bash
# hcl.sh goes in $PATH someplace
# author: haxwithaxe (me at haxwithaxe dot net)

if [[ `whoami` != 'root' ]] ;then
	echo 'Must be root, to execute.'
	exit 1
fi

hostname=`hostname`
hcldir=hcl.d
mkdir -p $hcldir

dump_progs=(
lsdev
lscpu
lshal
lsmod
lspci
lspcmcia
lsscsi
lssubsys
lsusb
dmesg
)

dump_hadware_info(){
	for p in $dump_progs ;do
		$p > ${hcldir}/${hostname}.${p}
	done
}

scrub_dump(){
	# scrub MACs
	sed -i 's/[A-Fa-f0-9]\:[A-Fa-f0-9]\:[A-Fa-f0-9]\:[A-Fa-f0-9]\:[A-Fa-f0-9]/00:00:00:00:00:00/g' $1
	# scrub ipv4 !!BROKEN!!
	sed -i '/127\.[0-9]\{1,3\}\.[0-9]\{1,3\}\.[0-9]\{1,3\}/! s/[0-9]\{1,3\}\.[0-9]\{1,3\}\.[0-9]\{1,3\}\.[0-9]\{1,3\}/1.1.1.1/g'
	# scrub ipv6
	#sed -i 's/[A-Fa-f0-9]\{0,2\}:[:A-Fa-f0-9]//g'

}

ask_yn(){
	yn='[y/n]'
	question=$1
	default=$2
	if [[ ${default,,} == 'y' ]] ;then
		yn='[Y/n]'
	elif [[ ${default,,} == 'n' ]] ;then
		yn='[y/N]'
	fi
	echo $question $yn
	read -n 1 answer
	if [[ ${answer,,} == 'y' ]] ;then
		return true
	elif [[ ${answer,,} == 'n' ]] ;then
		return false
	else
		echo "${answer} is not a valid option. Please try again."
		return ask_yn $question $default
	fi
}
ask_oneliner(){
	question=$1
	echo "${question} [press <enter> when done]"
	read answer
	return $answer
}

ask_multiline(){
	question=$1
	answer=''
	ui='junk'
	echo "${question} [press <enter> twice when done]"
	while [[ $ui != '' ]] ;do
		read ui
		answer="${answer}\n${ui}"
	done
	return $answer
}

ask_for_user_input_en(){
	box_info_make=ask_oneliner 'Please give the make of this computer, and add the actual manufacturer and/or parent company name if you know those.'
	box_info_model=ask_oneliner 'Please give the model of this computer as specifically as possible (full model numbers are often written on the bottom side of laptops or behind the battery or battery cover).'
	box_info_wireless=ask_oneliner 'Please give the make, model, and chipset (lspci or lsusb help to find this sometimes) of your wireless card that you used with byzantium. Also include the original manufacturer if you know that.'
	all_working=ask_yn 'Is everything working correctly?' 'y'
	if ! $all_working ;then
		what_broke=ask_multiline 'what is not working properly?'
		why_what_broke=ask multiline "Do you know why it doesn't work properly? If so please say why in as much detail as possible."
	fi
}

dump_hardware_info_en(){
	do_dump=ask_yn 'May we dump the output of hardware enumeration scripts for posting to public websites (anything that looks like a mac or IP address will be scrubbed before hand)?' 'y'
	if $do_dump ;then
		dump_harware_info
		scrub_dump
		package_loc=package_dump
		echo 'Thank you for your help. The hardware information dump is at "'${package_loc}'". You should review it and ensure that no personal data made it in. Once you have reviewed the hardware information dump please upload it to the Byzantium Hardware Compatability List (HCL) on our HCL wiki page ('${HCL_LINK}').'
	else
		send_answers=ask_yn 'May we post the answers to the questions we asked on our Hardware Compatibility List page?' 'y'
		if [[ $send_answers == 'y' ]] ;then
			report_loc=format_answers
			echo 'Thank you for your help. The information you provided is in "'${report_loc}'". Please review it and then upload it to the Byzantium Hardware Compatability List (HCL) on our HCL wiki page ('${HCL_LINK}').'
		fi
	fi
}
