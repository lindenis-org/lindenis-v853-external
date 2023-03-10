#!/bin/sh

IFNAME=$1
CMD=$2
echo "enter p2p-action.sh "
echo $IFNAME
echo $CMD
kill_daemon() {
    NAME=$1
    PF=$2
    echo "enter kill_daemon"
    if [ ! -r $PF ]; then
    return
    fi

    PID=`cat $PF`
    echo "$PID"
    if [ $PID -gt 0 ]; then
    if ps | grep -q $NAME; then
        kill -9 $PID
    fi
    fi
    rm $PF
}

if [ "$CMD" = "P2P-GROUP-STARTED" ]; then
    GIFNAME=$3
    echo "p2p-action.sh:$CMD"

    if [ "$4" = "GO" ]; then
    #kill_daemon dnsmasq /var/run/dnsmasq.pid
    ifconfig $GIFNAME 192.168.5.1 up
    sleep 1
    dnsmasq -C /etc/dnsmasq.conf
#    if ! dnsmasq -x /var/run/dnsmasq.pid \
#        -i $GIFNAME -l /var/run/dnsmasq.leases\
#        -F192.168.49.2,192.168.49.254,infinite; then
         # another dnsmasq instance may be running and blocking us; try to
         # start with -z to avoid that
#        dnsmasq -x /var/run/dnsmasq.pid \
#        -i $GIFNAME -l /var/run/dnsmasq.leases\
#        -F192.168.49.2,192.168.49.254,infinite --listen-address 192.168.49.1 -z -p 0
#   fi
    fi
    if [ "$4" = "client" ]; then
    udhcpc -i $GIFNAME
    fi
fi

if [ "$CMD" = "P2P-GROUP-REMOVED" ]; then
    GIFNAME=$3
    echo "p2p-action.sh:$CMD"
    if [ "$4" = "GO" ]; then
    #kill_daemon dnsmasq /var/run/dnsmasq.pid
    killall -9 dnsmasq
    ifconfig $GIFNAME 0.0.0.0
#    ifconfig $GIFNAME down
    fi
    if [ "$4" = "client" ]; then
    ifconfig $GIFNAME 0.0.0.0
#    ifconfig $GIFNAME down
    fi
fi
