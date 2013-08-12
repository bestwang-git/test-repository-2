#!/bin/bash
#
# compare the netconfcentral YANG files from Yuma
# and YumaPro to make sure they are the same

YUMA_NC=$HOME/swdev/yuma/netconf/modules/netconfcentral

YUMAPRO_NC=$HOME/swdev/ypwork/netconf/modules/netconfcentral

function diffyang {
   echo "************* Compare $1 ******************"
   echo "< = $YUMA_NC"
   echo "> = $YUMAPRO_NC"
   diff $YUMA_NC/$1 $YUMAPRO_NC/$1
}

diffyang toaster.yang
diffyang yuma-app-common.yang
diffyang yuma-arp.yang
diffyang yuma-interfaces.yang
diffyang yuma-mysession.yang
diffyang yuma-nacm.yang
diffyang yuma-ncx.yang
diffyang yuma-netconf.yang
diffyang yuma-proc.yang
diffyang yuma-system.yang
diffyang yuma-time-filter.yang
diffyang yuma-types.yang
diffyang yuma-xsd.yang


